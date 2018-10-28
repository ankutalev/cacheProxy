#pragma once

#include <pthread.h>
#include <vector>
#include <queue>

typedef void*(*Job)(void*);
struct WorkerData;

class ThreadPool {
public:
    ThreadPool();
    explicit ThreadPool (int workers);
    ~ThreadPool();
    void startAll();
    void addJob(Job job, void *jobArgs);
    void stopAll();
private:
    static const int DEFAULT_THREADS = 10;
    std::vector<pthread_t> threads;
    WorkerData *workerData;
};


