#ifndef __MUTUAL_SHMEM_HPP__
#define __MUTUAL_SHMEM_HPP__


#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <cstdint>
#include <fstream>
#include <cstdio>
#include <cstring>

#include <vector>
#include <string>
#include <algorithm>
#include <queue>


namespace MUTUAL_SHMEM_WITH_QUEUE
{

struct DataWrapper
{
    pthread_mutex_t lock;
    uint32_t process_count = 0;
    // 仅用于标记数据的起始地址，占位
    uint32_t data_start = 0;
};

class MutualSharedMemory
{
protected:
    pthread_mutex_t* shared_lock = nullptr;
    // 记录共享内存映射到中用户空间的地址
    void* shmem_addr = nullptr;
    int shmem_id = -1;
    size_t shmem_size = 0;
    std::string filepath;
    bool is_create = false;
public:
    static bool delShmem(const std::string& shmem_name)
    {
        if(shmem_name.empty()) return false;
        const size_t filepath_size = shmem_name.size() + 256u;
        char* filepath = new char[filepath_size];
        ::snprintf(filepath, filepath_size, "/tmp/%d-%s", static_cast<int>(shmem_name[0]), shmem_name.c_str());
        std::string filepath_str(filepath);
        delete[] filepath;
        
        bool file_exists = false;
        {
            std::ifstream file_stream(filepath_str, std::ios_base::in);
            file_exists = file_stream.good();
            file_stream.close();
        }

        if(!file_exists)
        {
            std::ofstream file_out_stream(filepath_str, std::ios_base::app);
            if(file_out_stream.is_open())
                file_out_stream.close();
        }

        key_t key = ::ftok(filepath_str.c_str(), 0);
        int shmem_id = ::shmget(key, 0, 0666 | IPC_EXCL);
        std::remove(filepath_str.c_str());
        if(-1 == shmem_id) return false;
        return 0 == shmctl(shmem_id, IPC_RMID, NULL);
    }


    MutualSharedMemory() = delete;
    MutualSharedMemory(MutualSharedMemory&& other_mem) = delete;

    explicit MutualSharedMemory(const std::string& shmem_name, size_t data_size)
    {
        if(shmem_name.empty()) return;
        const size_t filepath_size = shmem_name.size() + 256u;
        char* filepath = new char[filepath_size];
        ::snprintf(filepath, filepath_size, "/tmp/%d-%s", static_cast<int>(shmem_name[0]), shmem_name.c_str());
        std::string filepath_str(filepath);
        delete[] filepath;

        {
            std::ifstream file_stream(filepath_str, std::ios_base::in);
            bool file_exists = file_stream.good();
            file_stream.close();
            if(!file_exists)
            {
                std::ofstream file_out_stream(filepath_str, std::ios_base::app);
                if(file_out_stream.is_open())
                    file_out_stream.close();
            }
        }

        shmem_size = sizeof(DataWrapper) - sizeof(((DataWrapper*)0)->data_start) + data_size;
        key_t key = ::ftok(filepath_str.c_str(), 0);
        int ret_shmem_id = ::shmget(key, shmem_size, 0666 | IPC_CREAT | IPC_EXCL);
        bool is_create = true;
        if(-1 == ret_shmem_id)
        {
            ret_shmem_id = ::shmget(key, 0, 0666 | IPC_CREAT);
            is_create = false;
        }

        if(-1 == ret_shmem_id) return;
        this->shmem_id = ret_shmem_id;
        this->is_create = is_create;
        this->shmem_addr = ::shmat(this->shmem_id, NULL, 0);
        if((void*)(-1) == this->shmem_addr)
        {
            this->shmem_addr = nullptr;
            return;
        }

        this->shared_lock = (pthread_mutex_t *)shmem_addr;
        if(is_create)
        {
            // 初始化内存
            ::bzero(this->shmem_addr, shmem_size);
            // 初始化进程间共享互斥锁
            pthread_mutexattr_t ma;
            pthread_mutexattr_init(&ma);
            pthread_mutexattr_setrobust(&ma, ::PTHREAD_MUTEX_ROBUST);
            pthread_mutexattr_setpshared(&ma, ::PTHREAD_PROCESS_SHARED);
            pthread_mutex_init(this->shared_lock, &ma);
            ((DataWrapper*)(shmem_addr))->process_count = 0;
        }

        if(this->lock())
        {
            ((DataWrapper*)(this->shmem_addr))->process_count += 1;
            this->unlock();
        }
    }

    MutualSharedMemory(const MutualSharedMemory& other_mem)
    {
        this->shmem_id = other_mem.shmem_id;
        if(-1 == this->shmem_id) return;
        this->shmem_addr = ::shmat(this->shmem_id, NULL, 0);
        if((void*)(-1) == this->shmem_addr)
        {
            this->shmem_addr = nullptr;
            return;
        }
        this->shared_lock = other_mem.shared_lock;
        this->shmem_size = other_mem.shmem_size;
        this->is_create = false;
        this->filepath = std::string(other_mem.filepath.c_str());

        if(this->lock())
        {
            ((DataWrapper*)(this->shmem_addr))->process_count += 1;
            this->unlock();
        }
    }

    MutualSharedMemory& operator=(const MutualSharedMemory& other_mem)
    {
        
        this->shmem_id = other_mem.shmem_id;
        if(-1 == this->shmem_id) return *this;
        this->shmem_addr = ::shmat(this->shmem_id, NULL, 0);
        if((void*)(-1) == this->shmem_addr) 
        {
            this->shmem_addr = nullptr;
            return *this;
        }
        this->shared_lock = other_mem.shared_lock;
        this->shmem_size = other_mem.shmem_size;
        this->is_create = false;
        this->filepath = std::string(other_mem.filepath.c_str());

        if(this->lock())
        {
            ((DataWrapper*)(this->shmem_addr))->process_count += 1;
            this->unlock();
        }
        return *this;
    }

    void* get_shared_data_pointer() const noexcept
    {
        if(!this->shmem_addr) return nullptr;
        return &(((DataWrapper*)(shmem_addr))->data_start);
    }

    

    bool lock()
    {
        if(!shared_lock) return false;
        return 0 == pthread_mutex_lock(shared_lock);
    }

    bool unlock()
    {
        if(!shared_lock) return false;
        return 0 == pthread_mutex_unlock(shared_lock);
    }

    virtual ~MutualSharedMemory()
    {
        if(-1 == shmem_id) return;
        int process_count = 0;
        if(this->lock())
        {
            ((DataWrapper*)(shmem_addr))->process_count -= 1;
            process_count = ((DataWrapper*)(shmem_addr))->process_count;
            this->unlock();
        }
        
        int shm_nattch = 65535;
        struct shmid_ds shmem_stat;
        if(0 == shmctl(shmem_id, IPC_STAT, &shmem_stat))
        {
            shm_nattch = shmem_stat.shm_nattch;
        }

        if(process_count <= 0 && shm_nattch < 2)
        {
            std::remove(filepath.c_str());
            pthread_mutex_destroy(shared_lock);
            shared_lock = nullptr;
            shmdt(shmem_addr);
            shmctl(shmem_id, IPC_RMID, NULL);
        }
        else
        {
            shmdt(shmem_addr);
        }
    }

};


template<int _max_size, typename Type = int, typename Function = std::less<Type> >
class MutualShmemWithPriorityQueue : public MutualSharedMemory
{
protected:
    Function comp;
    struct Data
    {
        Type data[_max_size];
        int length = 0;
        size_t max_size = _max_size;
    };
    Data* data_ptr = nullptr;

public:
    using ValueType=Type;

    MutualShmemWithPriorityQueue() = delete;

    explicit MutualShmemWithPriorityQueue(const std::string& shmem_name, const std::vector<Type>& init_data, const Function& func) : MutualSharedMemory(shmem_name, sizeof(Data)), comp(func)
    {
        data_ptr = static_cast<Data*>(get_shared_data_pointer());
        if(lock())
        {
            data_ptr->max_size = is_create ? _max_size : data_ptr->max_size;
            this->unlock();
        }

        if(is_create && !init_data.empty())
        {
            size_t write_data_size = std::min(data_ptr->max_size, init_data.size());
            if(lock())
            {
                data_ptr->length = write_data_size;
                for(int i = 0; i < write_data_size; ++i)
                    data_ptr->data[i] = init_data[i];
                std::make_heap(data_ptr->data, data_ptr->data + write_data_size, comp);
                unlock();
            }
        }
    }
    
    template<typename _Type = Type, typename _Func = Function, typename = typename std::enable_if<
        std::__and_< std::is_default_constructible<_Type>, std::is_default_constructible<_Func>>::value>::Type>
    explicit MutualShmemWithPriorityQueue(const std::string& shmem_name) : MutualShmemWithPriorityQueue(shmem_name, std::vector<Type>(), Function()) {}

    template<typename _Func = Function, typename = typename std::enable_if<std::is_default_constructible<_Func>::value>::Type>
    explicit MutualShmemWithPriorityQueue(const std::string& shmem_name, const std::vector<Type>& init_data) : MutualShmemWithPriorityQueue(shmem_name, init_data, Function()) {}

    MutualShmemWithPriorityQueue(MutualShmemWithPriorityQueue&&) = delete;
    MutualShmemWithPriorityQueue(const MutualShmemWithPriorityQueue& other_mem) : MutualSharedMemory(other_mem), comp(other_mem.comp)
    {
        this->data_ptr = other_mem.data_ptr;
    }

    MutualShmemWithPriorityQueue& operator=(MutualShmemWithPriorityQueue& other_mem)
    {
        MutualShmemWithPriorityQueue::operator=(other_mem);
        this->data_ptr = other_mem.data_ptr;
        this->comp = other_mem.comp;
        return *this;
    }

    bool add_item(const Type& item)
    {
        if(!lock())
        {
            unlock();
            return false;
        }
        if(data_ptr->length > data_ptr->max_size) return false;

        data_ptr->data[data_ptr->length] = item;
        ++data_ptr->length;
        // 执行shiftup
        std::push_heap(data_ptr->data, data_ptr->data + data_ptr->length, comp);
        unlock();
        return true;
    }

    bool pop_item(Type& ret_val)
    {
        if(!lock())
        {
            unlock();
            return false;
        }

        if(data_ptr->length <= 0) return false;

        std::pop_heap(data_ptr->data, data_ptr->data + data_ptr->length, comp);
        --data_ptr->length;
        ret_val = data_ptr->data[data_ptr->length];
        unlock();
        return true;
    }


    std::pair<bool, Type> _pop_item_for_python()
    {
        if(!lock())
        {
            unlock();
            return {false, data_ptr->data[0]};
        }
        if(data_ptr->length <= 0) return {false, data_ptr->data[0]};

        std::pop_heap(data_ptr->data, data_ptr->data + data_ptr->length, comp);
        --data_ptr->length;
        auto& ret_val = data_ptr->data[data_ptr->length];
        unlock();
        return {true, ret_val};
    }

    int length()
    {
        if(!lock())
        {
            unlock();
            return 0;
        }

        int length = data_ptr->length;
        unlock();
        return length;
    }

    virtual ~MutualShmemWithPriorityQueue()
    {
        data_ptr = nullptr;
    }
};

} // namespace MUTUAL_SHMEM_WITH_QUEUE


#endif