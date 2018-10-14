#include <stdexcept>
#include "ThreadPool.h"
#include <iostream>

struct WorkerData {
     WorkerData() : mutex(PTHREAD_MUTEX_INITIALIZER),condVar(PTHREAD_COND_INITIALIZER) {}
    ~WorkerData() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&condVar);
    }
    std::queue<std::pair<Job,void*>> jobs;
    pthread_mutex_t mutex;
    pthread_cond_t condVar;
};

ThreadPool::ThreadPool() : ThreadPool(DEFAULT_THREADS) {}

ThreadPool::ThreadPool(int workers) : threads(workers), workerData(new WorkerData)  {}

ThreadPool::~ThreadPool() {
    delete workerData;
}

void ThreadPool::addJob(Job job, void *jobArgs) {
    pthread_mutex_lock(&workerData->mutex);
    workerData->jobs.push(std::make_pair(job,jobArgs));
    pthread_cond_signal(&workerData->condVar);
    pthread_mutex_unlock(&workerData->mutex);
}

void ThreadPool::stopAll() {
    for (unsigned long thread : threads) {
        pthread_cancel(thread);
    }
}

//void *ThreadPool::workerBody(void* workerIndex) {
//    while(1) {
//        pthread_mutex_lock(&mutex);
//        while (jobs.empty())
//            pthread_cond_wait(&condVar, &mutex);
//        auto job = jobs.front(); // first - func,second - args
//        jobs.pop();
//        pthread_mutex_unlock(&mutex);
//        job.first(job.second);
//    }
//    return nullptr;
//}



static void* workerBody(void* workerData) {
    auto wData = (WorkerData*) workerData;
    while (true) {
        pthread_mutex_lock(&wData->mutex);
        while (wData->jobs.empty())
            pthread_cond_wait(&wData->condVar, &wData->mutex);
        auto job = wData->jobs.front(); // first - func,second - args
        wData->jobs.pop();
        pthread_mutex_unlock(&wData->mutex);
        job.first(job.second);
    }
    return nullptr;
}


void ThreadPool::startAll() {
    for (unsigned long &thread : threads) {
//        auto err = pthread_create(&thread, nullptr, &ThreadPool::workerBody, (void*)workerIndex);
        auto err = pthread_create(&thread, nullptr, workerBody, workerData);
        if (err)
            throw std::runtime_error("Can't create threads!");
    }
//    for (unsigned long thread : threads) {
//                auto err = pthread_detach(thread);
//        if (err)
//            throw std::runtime_error("Can't detach threads!");
//    }
}