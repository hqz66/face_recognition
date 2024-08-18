
#ifndef __MAT_QUEUE_H__
#define __MAT_QUEUE_H__

#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

class CMatQueue
{
private:
    condition_variable m_producer_cond;
    condition_variable m_consumer_cond;
    mutex m_mutex;
    queue<Mat> m_queue;
    int m_max_size;

    static mutex m_instance_mutex;
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



mutex CMatQueue::m_instance_mutex;
CMatQueue *CMatQueue::instance = nullptr;
CMatQueue *CMatQueue::getInstance(int max_size)
{
    static CMatQueue instance(max_size);
    return &instance;
}
CMatQueue::CMatQueue() : m_max_size(50) {}
CMatQueue::CMatQueue(int max_size) : m_max_size(max_size) {}
CMatQueue::~CMatQueue()
{
    while (!m_queue.empty())
    {
        m_queue.pop();
    }
}
void CMatQueue::push(const Mat &picture)
{
    unique_lock<mutex> lock(m_mutex);
    while (m_queue.size() >= m_max_size)
    {
        m_producer_cond.wait(lock);
    }
    m_queue.push(picture);
    m_consumer_cond.notify_all();
}

void CMatQueue::pop(Mat &picture)
{
    unique_lock<mutex> lock(m_mutex);
    while (m_queue.empty())
    {
        m_consumer_cond.wait(lock);
    }
    picture = m_queue.front();
    m_queue.pop();
    m_producer_cond.notify_all();
}

bool CMatQueue::pop_timeout(Mat &picture, int timeout)
{
    unique_lock<mutex> lock(m_mutex);
    while (m_queue.empty())
    {
        auto ret = m_consumer_cond.wait_for(lock, chrono::seconds(timeout));
        if (ret == cv_status::timeout)
        {
            fprintf(stderr, "Thread: Timed out waiting for condition.\n");
            return false;
        }
    }
    picture = m_queue.front();
    m_queue.pop();
    m_producer_cond.notify_all();
    return true;
}

int CMatQueue::get_size()
{
    unique_lock<mutex> lock(m_mutex);
    return m_queue.size();
}

void CMatQueue::set_size(int max_size)
{
    unique_lock<mutex> lock(m_mutex);
    m_max_size = max_size;
}

#endif
