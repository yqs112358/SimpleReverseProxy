#include "threadpool.h"
#include "pthread.h"
#include <iostream>
using namespace std;

// 注意！如果出现嵌套加锁，必须严格按照先taskQueueLocker后threadListLocker的顺序，以免死锁

// 线程池中执行的线程函数
void* funcInThreadPool(void* data)
{
    ThreadPool* pool = (ThreadPool*)data;   // data为所属线程池的this指针
    pthread_t selfId = pthread_self();      // 当前线程ID

    pool->threadListLocker.lock();
    ++pool->leftFreeThreads;    // 空闲线程计数+1
    pool->threadListLocker.unlock();

    while(true)
    {
        pool->taskQueueLocker.lock();
        // 检查是否有新任务可以执行
        // 此时taskQueueLocker是锁定状态
        if(pool->taskQueue.size() != 0)
        {
            // 有新任务
            TaskData task = pool->taskQueue.front();
            pool->taskQueue.pop();

            pool->threadListLocker.lock();
            --pool->leftFreeThreads;    // 空闲线程计数-1
            // cout << "[线程池] 线程 #" << selfId % 10000 << " 执行新任务。当前线程池共拥有："
            //         << pool->threadList.size() << "个线程" << endl;
            pool->threadListLocker.unlock();

            pool->taskQueueLocker.unlock();     // 解锁taskQueue
            task.func(task.data);       // 执行任务

            pool->threadListLocker.lock();
            // cout << "[线程池] 线程 #" << selfId % 10000 << " 执行任务结束" << endl;
            ++pool->leftFreeThreads;    // 执行完毕，空闲线程计数+1
            pool->threadListLocker.unlock();
            continue;
        }

        // 运行到这里时，taskQueueLocker一定处于锁定状态
        // 没有新任务，等待条件变量唤醒
        // wait_for过程中taskQueueLocker处于解锁状态，允许其他线程使用taskQueue
        auto result = pool->cond.wait(pool->taskQueueLocker, pool->maxWait); 

        // wait_for返回时，此时taskQueueLocker被重新锁定
        if(result == ConditionVar::Timeout)
        {
            // 等待超时，检查此线程是否需要退出
            pool->threadListLocker.lock();
            if((int)pool->threadList.size() > pool->minThreads)
            {
                // 线程可以退出，空闲线程计数-1
                --pool->leftFreeThreads;    
                // 将自己的ID从threadList中移除
                auto& list = pool->threadList;
                for(auto id=list.begin(); id!=list.end(); ++id)
                    if(*id == selfId)
                    {
                        list.erase(id);
                        break;
                    }
                // cout << "[线程池] 线程 #" << selfId % 10000 << " 等待超时退出。当前线程池共拥有："
                //      << pool->threadList.size() << "个线程" << endl;
                pool->threadListLocker.unlock();

                // 线程退出，节省资源
                pool->taskQueueLocker.unlock();
                pthread_exit(NULL);
            }
            pool->threadListLocker.unlock();
            
            // 线程不能退出，继续循环，空闲线程计数不变
            pool->taskQueueLocker.unlock();
            continue;
        }
        else
        {
            // 线程是被条件变量notify唤醒的
            // 此时taskQueueLocker是锁定状态
            // cout << "[线程池] 线程 #" << selfId % 10000 << " 被条件变量notify唤醒" << endl;
            pool->taskQueueLocker.unlock();
            continue;   //回到函数头部，检查是否有新任务可以执行
        }
    }
}

ThreadPool::ThreadPool(int minThreads, int maxThreads, int maxWait)
{
    this->minThreads = minThreads;
    this->maxThreads = maxThreads;
    this->maxWait = maxWait;
    this->leftFreeThreads = 0;
    
    // 初始化minThreads个线程到线程池
    for(int i=0; i<minThreads; ++i)
    {
        pthread_t threadId;
        int res = pthread_create(&threadId, NULL, funcInThreadPool, this);
        // 将this传入以便线程池线程读写成员
        if(res != 0)    
        {
            // cout << "[线程池] 创建线程池失败，错误码：" << res << endl;
            exit(-1);
        }
        pthread_detach(threadId);

        threadListLocker.lock();
        threadList.push_back(threadId);
        // cout << "[线程池] 新增工作线程 #" << threadId % 10000
        //     << " ，当前线程池共拥有：" << threadList.size() << "个线程" << endl;
        threadListLocker.unlock();
    }
    // cout << "[线程池] 线程池初始化完毕。当前线程池共有" << threadList.size() << "个线程" << endl;
}
    
ThreadPool::~ThreadPool()
{
    // cout << "[线程池] 线程池析构" << endl;
    taskQueueLocker.lock();
    threadListLocker.lock();

    // 杀死每一个存活的线程
    for(int i=0; i<(int)threadList.size(); ++i)
        pthread_cancel(threadList[i]);

    threadListLocker.unlock();
    taskQueueLocker.unlock();
}

bool ThreadPool::addTask(TaskFunction taskFunction, void* data)
{
    // cout << "[线程池] 有新任务到达" << endl;

    // 插入新任务到队列
    taskQueueLocker.lock();
    taskQueue.push({taskFunction, data});

    // 检查是否需要新增线程
    threadListLocker.lock();
    if(this->leftFreeThreads < (int)taskQueue.size() && (int)threadList.size() < this->maxThreads)
    {
        // 当前空闲线程数小于总等待任务数，并且可以新增
        pthread_t threadId;
        int res = pthread_create(&threadId, NULL, funcInThreadPool, this);
        // 将this传入以便线程池线程读写成员
        if(res != 0)    
        {
            // cout << "[线程池] 创建新线程失败，错误码：" << res << endl;
            exit(-1);
        }
        pthread_detach(threadId);

        threadList.push_back(threadId);
        // cout << "[线程池] 新增工作线程 #" << threadId % 10000
        //     << " ，当前线程池共拥有：" << threadList.size() << "个线程" << endl;
    }
    threadListLocker.unlock();
    taskQueueLocker.unlock();

    // 条件变量触发，唤醒任意一个线程执行任务
    this->cond.notifyOne();
    return true;
}