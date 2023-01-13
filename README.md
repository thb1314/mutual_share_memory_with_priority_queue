# Shared Memory with mutexed

## 介绍

为解决单机多卡单个推理任务应该用哪张卡的问题，本项目提出一种“共享内存+进程间互斥锁+优先队列”的解决方案。  
假设每个任务都是依托于独立进程来运行，该解决方案在保证共享内存互斥访问的同时，旨在每个任务分配一个卡的id，采用优先队列来保证任务均匀分配。

本项目采用c++编写，主要采用linux应用层API+借鉴对STL优先队列实现，并针对特定场景采用pybind11封装操作api对python用户暴露接口。

读者按需自定义更改模板中类型和比较函数，从而封装满足自己项目需求的python接口。

## 预先安装

```
g++ >= 7.3
python3.7
make
```

python依赖安装
```bash
pip install -r requirements.txt
```

## 安装指令

just make  

```bash
make
```
