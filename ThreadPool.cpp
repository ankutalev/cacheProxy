#include <iostream>
#include <stdexcept>
#include "ThreadPool.h"

struct WorkerData {
    WorkerData() {
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&condVar, NULL);
    }
    ~WorkerData() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&condVar);
    }

    std::queue<std::pair<Job, void *> > jobs;
    pthread_mutex_t mutex;
    pthread_cond_t condVar;
};

ThreadPool::ThreadPool() : threads(DEFAULT_THREADS), workerData(new WorkerData) {

}

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
    for (int i = 0; i < threads.size(); ++i) {
        pthread_cancel(threads[i]);
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
    WorkerData *wData = (WorkerData *) workerData;
    while (true) {
        pthread_mutex_lock(&wData->mutex);
        while (wData->jobs.empty())
            pthread_cond_wait(&wData->condVar, &wData->mutex);
        std::pair<Job, void *> job = wData->jobs.front(); // first - func,second - args
        wData->jobs.pop();
        pthread_mutex_unlock(&wData->mutex);
        job.first(job.second);
    }
    return NULL;
}


void ThreadPool::startAll() {
    for (int i = 0; i < threads.size(); ++i) {
        int err = pthread_create(&threads[i], NULL, workerBody, workerData);
        if (err)
            throw std::runtime_error("Can't create threads!");
    }
}
