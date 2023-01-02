#ifndef CONDITION_VAR_BY_YQ
#define CONDITION_VAR_BY_YQ

#include <pthread.h>
#include <errno.h>
#include "mutex.h"

class ConditionVar
{
private:
    pthread_cond_t cond;

public:
    static const int Timeout = 0;
    static const int NotTimeout = 1;

    ConditionVar()
    {
        pthread_cond_init(&cond, NULL);
    }

    ~ConditionVar()
    {
        pthread_cond_destroy(&cond);
    }

    // 调用前 queueMutex 需要处于锁定状态，wait返回后 queueMutex 同样处于锁定状态
    bool wait(Mutex &queueMutex, int waitSeconds)
    {
        // 计算等待时间
        struct timespec waitTime;
        clock_gettime(CLOCK_REALTIME, &waitTime);                                       
        waitTime.tv_sec += waitSeconds;

        // wait过程中 queueMutex 处于解锁状态
        // wait返回后 queueMutex 处于锁定状态
        if(pthread_cond_timedwait(&cond, &(queueMutex.getLocker()), &waitTime) == ETIMEDOUT)
        {
            return Timeout;
        }
        else return NotTimeout;
    }

    // 唤醒一个线程
    void notifyOne()
    {
        pthread_cond_signal(&cond);
    }

    // 唤醒所有线程
    void notifyAll()
    {
        pthread_cond_broadcast(&cond);
    }

    pthread_cond_t& getCondVar()
    {
        return cond;
    }
};
#endif