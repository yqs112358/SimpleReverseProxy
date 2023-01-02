#ifndef MUTEX_BY_YQ
#define MUTEX_BY_YQ

#include <pthread.h>

class Mutex
{
private:
    pthread_mutex_t mutex;
    
public:
    Mutex()
    {
        pthread_mutex_init(&mutex, NULL);
    }

    ~Mutex()
    {
        pthread_mutex_destroy(&mutex);
    }
    
    int lock()
    {
        return pthread_mutex_lock(&mutex);
    }

    int unlock()
    {
        return pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_t& getLocker()
    {
        return mutex;
    }
};
#endif