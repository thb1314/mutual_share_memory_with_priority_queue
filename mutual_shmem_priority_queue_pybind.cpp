#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "mutual_shmem.hpp"


namespace MutualShmemPriorityIndexedQueuePyBind
{
using value_with_index = std::pair<int, int>;

auto cmp_func = [](const value_with_index& child, const value_with_index& parent)
{
    if(parent.first < child.first)
        return true;
    else if(parent.first == child.first)
        return parent.second < child.second;
    else
        return false;
};

using cmp_func_type = decltype(cmp_func);
using _MutualShmemPriorityIndexedQueue = MUTUAL_SHMEM_WITH_QUEUE::MutualShmemWithPriorityQueue<1024, value_with_index, cmp_func_type>;

class MutualShmemPriorityIndexedQueue : public _MutualShmemPriorityIndexedQueue
{
public:
    using _MutualShmemPriorityIndexedQueue::_MutualShmemPriorityIndexedQueue;
    explicit MutualShmemPriorityIndexedQueue(const std::string& shmem_name, const std::vector<value_with_index>& init_data) : _MutualShmemPriorityIndexedQueue(shmem_name, init_data, cmp_func) {}
    explicit MutualShmemPriorityIndexedQueue(const std::string& shmem_name) : _MutualShmemPriorityIndexedQueue(shmem_name, std::vector<value_with_index>(), cmp_func) {}
    

    std::pair<bool, value_with_index> get_item_with_inc()
    {
        std::pair<bool, value_with_index> ret_val({false, {0 ,-1}});

        if(!lock())
        {
            unlock();
            return ret_val;
        }

        if(data_ptr->length <= 0) return ret_val;

        std::pop_heap(data_ptr->data, data_ptr->data + data_ptr->length, comp);
        --data_ptr->length;
        ret_val.second = data_ptr->data[data_ptr->length];
        ret_val.first = true;
        ret_val.second.first += 1;

        data_ptr->data[data_ptr->length] = ret_val.second;
        ++data_ptr->length;
        // 执行 shift_up
        std::push_heap(data_ptr->data, data_ptr->data + data_ptr->length, comp);
        unlock();
        return ret_val;
    }

};

};

namespace py = pybind11;
PYBIND11_MODULE(shmem_with_priority_queue, m) {

    using MutualShmemPriorityIndexedQueuePyBind::MutualShmemPriorityIndexedQueue;
    m.def("del_shared_memory", &MutualShmemPriorityIndexedQueue::delShmem, "delete shared memory by name", py::arg("shmem_name"));

    py::class_<MutualShmemPriorityIndexedQueue>(m, "MutualShmemPriorityIndexedQueue")
        .def(py::init<>([](std::string shmem_name, std::vector<MutualShmemPriorityIndexedQueue::ValueType> init_data) {
            return std::unique_ptr<MutualShmemPriorityIndexedQueue>(new MutualShmemPriorityIndexedQueue(shmem_name, init_data));
        }))
        .def(py::init<>([](std::string shmem_name) {
            return std::unique_ptr<MutualShmemPriorityIndexedQueue>(new MutualShmemPriorityIndexedQueue(shmem_name));
        }))
        .def("add_item", &MutualShmemPriorityIndexedQueue::add_item, "add item to queue", py::arg("item"))
        .def("pop_item", &MutualShmemPriorityIndexedQueue::_pop_item_for_python, "pop item")
        .def("get_item_with_inc", &MutualShmemPriorityIndexedQueue::get_item_with_inc, "get top item of heap and inscrease its count value")
        .def("length", &MutualShmemPriorityIndexedQueue::length, "get length of queue data")
        .def("lock", &MutualShmemPriorityIndexedQueue::lock, "shared memory lock")
        .def("unlock", &MutualShmemPriorityIndexedQueue::unlock, "unlock")
        .def("__copy__", [](const MutualShmemPriorityIndexedQueue &self){
            return MutualShmemPriorityIndexedQueue(self);
        })
        .def("__deepcopy__", [](const MutualShmemPriorityIndexedQueue &self, py::dict){
            return MutualShmemPriorityIndexedQueue(self);
        });

}
