import shmem_with_priority_queue
import time
import os
import random
from multiprocessing import Process


def process_func():
    q = shmem_with_priority_queue.MutualShmemPriorityIndexedQueue("shmem", [(0, i) for i in range(8)])

    for i in range(5):
        start_time = time.perf_counter()
        take_num = random.randint(1, 20)
        id_list = []
        for j in range(take_num):
            ret_bool, val = q.get_item_with_inc()
            while not ret_bool:
                ret_bool, val = q.get_item_with_inc()
            count, index = val
            id_list.append(index)
        time.sleep(0.001)
        time_count = time.perf_counter() - start_time
        time_count = round(time_count * 1000, 3)
        print("time count", time_count, 'pid', os.getpid(), "id_list", id_list)


if __name__ == "__main__":
    process_list = []

    for i in range(5):
        p = Process(target=process_func)
        process_list.append(p)
    
    for p in process_list:
        p.start()

    for p in process_list:
        p.join()

    
    print("shmem_with_priority_queue.del_shared_memory(\"test_mem\")", shmem_with_priority_queue.del_shared_memory("test_mem"))
    q1 = shmem_with_priority_queue.MutualShmemPriorityIndexedQueue("test_mem")
    q2 = q1
    print("shmem_with_priority_queue.del_shared_memory(\"test_mem\")", shmem_with_priority_queue.del_shared_memory("test_mem"))

