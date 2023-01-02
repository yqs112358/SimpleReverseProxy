/* 
 * 规定任务函数均为如下原型：void TaskFunction(void* data);
 * 使用方法： pool.addTask(TaskFunction, data);
 * 
 * 调度算法设计思路如下：
 *
 * 1. 初始状态下线程池容纳 minThreads 个线程
 * 
 * 2. 有任务到达时，进入任务队列
 * 
 * 3. 接到新任务时，如果当前空闲线程数小于总等待任务数，且线程池中线程数未达到上限 maxThreads，
 *    则增加新的线程到线程池中，并执行队列中的新任务；
 *    如果线程池中线程数已达到上限，则将新任务放入等待队列，等待有线程空闲后再来执行。
 * 
 * 4. 每个线程完成任务后，检查队列中是否新的有未完成的任务，如果有则执行，否则进入空闲状态等待。
 *    如果空闲等待超过maxWait秒未被notify，且线程池中线程数量大于最小值 minThreads，
 *    则此线程释放自身并退出，以降低资源消耗
 *
*/

#ifndef THREADPOOL_BY_YQ
#define THREADPOOL_BY_YQ

#include <vector>
#include <queue>
#include "mutex.h"
#include "condition_var.h"

// 任务函数原型
using TaskFunction = void (*)(void *);
// 任务列表数据类
struct TaskData
{
    TaskFunction func;
    void* data;
};


class ThreadPool
{
private:
    // 线程函数设友元以便访问私有成员
    friend void* funcInThreadPool(void* data);
    // 基本参数
    int minThreads, maxThreads, maxWait;
    // 剩余空闲线程数
    int leftFreeThreads;
    // 有效线程列表
    std::vector<pthread_t> threadList;
    // 任务列表
    std::queue<TaskData> taskQueue;
    // 条件变量
    ConditionVar cond;
    // 锁
    Mutex threadListLocker;    // 负责锁线程列表、空闲线程数
    Mutex taskQueueLocker;     // 负责锁任务列表、条件变量

public:
    ThreadPool(int minThreads = 3, int maxThreads = 10, int maxWait = 5);
    ~ThreadPool();

    // 向线程池添加一项任务
    bool addTask(TaskFunction taskFunction, void* data);
};
#endif