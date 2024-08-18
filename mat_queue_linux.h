
#ifndef __MAT_QUEUE_H__
#define __MAT_QUEUE_H__

#include <iostream>
#include <queue>
#include <pthread.h>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

class CMatQueue
{
private:
    pthread_cond_t m_producer_cond;
    pthread_cond_t m_consumer_cond;
    pthread_mutex_t m_mutex;
    queue<Mat> m_queue;
    int m_max_size;

    static CMatQueue *instance;
    CMatQueue();
    CMatQueue(int max_size);
    ~CMatQueue();

public:
    static CMatQueue *getInstance(int max_size = 100);
    void push(const Mat &picture);
    void pop(Mat &picture);
    bool pop_timeout(Mat &picture, int timeout);
    int get_size();
    void set_size(int max_size);
};

CMatQueue *CMatQueue::instance = nullptr;
CMatQueue *CMatQueue::getInstance(int max_size)
{
    static CMatQueue instance(max_size);
    return &instance;
}
CMatQueue::CMatQueue()
{
    m_max_size = 50;
    m_mutex = PTHREAD_MUTEX_INITIALIZER;
    m_producer_cond = PTHREAD_COND_INITIALIZER;
    m_consumer_cond = PTHREAD_COND_INITIALIZER;
}

CMatQueue::CMatQueue(int max_size)
{
    m_max_size = max_size;
    m_mutex = PTHREAD_MUTEX_INITIALIZER;
    m_producer_cond = PTHREAD_COND_INITIALIZER;
    m_consumer_cond = PTHREAD_COND_INITIALIZER;
}
CMatQueue::~CMatQueue()
{
    while (!m_queue.empty())
    {
        m_queue.pop();
    }
}
void CMatQueue::push(const Mat &picture)
{
    pthread_mutex_lock(&m_mutex);
    while (m_queue.size() >= m_max_size)
    {
        pthread_cond_wait(&m_producer_cond, &m_mutex);
    }
    m_queue.push(picture);
    pthread_cond_broadcast(&m_consumer_cond);
    pthread_mutex_unlock(&m_mutex);
}

void CMatQueue::pop(Mat &picture)
{
    pthread_mutex_lock(&m_mutex);
    while (m_queue.empty())
    {
        pthread_cond_wait(&m_consumer_cond, &m_mutex);
    }
    picture = m_queue.front();
    m_queue.pop();
    pthread_cond_broadcast(&m_producer_cond);
    pthread_mutex_unlock(&m_mutex);
}

bool CMatQueue::pop_timeout(Mat &picture, int timeout)
{
    struct timespec ts;
    // CLOCK_MONOTONIC不受系统时间影响
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += timeout;

    pthread_mutex_lock(&m_mutex);
    while (m_queue.empty())
    {
        int ret = pthread_cond_timedwait(&m_consumer_cond, &m_mutex, &ts);
        if (ret == 0)
        {
        }
        else if (ret == ETIMEDOUT)
        {
            pthread_mutex_unlock(&m_mutex);
            fprintf(stderr, "Thread: Timed out waiting for condition.\n");
            return false;
        }
        else
        {
            pthread_mutex_unlock(&m_mutex);
            fprintf(stderr, "pthread_cond_timedwait failed.\n");
            return false;
        }
    }
    if (!m_queue.empty())
    {
        picture = m_queue.front();
        m_queue.pop();
        pthread_cond_broadcast(&m_producer_cond);
    }
    pthread_mutex_unlock(&m_mutex);
    return true;
}

int CMatQueue::get_size()
{
    int count = 0;
    pthread_mutex_lock(&m_mutex);
    count = m_queue.size();
    pthread_mutex_unlock(&m_mutex);
    return count;
}

void CMatQueue::set_size(int max_size)
{
    pthread_mutex_lock(&m_mutex);
    m_max_size = max_size;
    pthread_mutex_unlock(&m_mutex);
}


#endif
