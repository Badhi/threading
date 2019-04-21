#include <mutex>
#include <thread>
#include <queue>
#include <cassert>
#include <condition_variable>

#include <iostream>
#include <fstream>
#include <string>

#define LOG(message) \
    std::cout << std::hash<std::thread::id>()(std::this_thread::get_id()) << " : " << message << std::endl

template <typename T>
class ThreadSafeQueue
{
private:
    std::queue<T*> q;
    std::mutex m; 
public:
    void push(T * data)
    {
        std::lock_guard<std::mutex> l(m);
        q.push(data);
    }
    T * back()
    {
        std::lock_guard<std::mutex> l(m);
        return q.back();
    }
    T * front()
    {
        std::lock_guard<std::mutex> l(m);
        return q.front();
    }
    void pop()
    {
        std::lock_guard<std::mutex> l(m);
        return q.pop();
    }
    bool empty()
    {
        std::lock_guard<std::mutex> l(m);
        return q.empty();
    }
    
};

struct Data
{
    uint size;
    char * buffer;
};

class WorkerCallback
{
public:
    virtual void onData(Data * d) = 0;
    virtual WorkerCallback * clone() = 0;

};

class WorkerCallbackImpl : public WorkerCallback
{
    std::ofstream file;
public:
    WorkerCallbackImpl() : file(std::string("ThreadFile_") + 
                std::to_string(std::hash<std::thread::id>()(std::this_thread::get_id())))
    {
    }
    void onData(Data * d) override 
    {
        file << "Data recieved : "<< d << std::endl;
        for(uint i = 0; i < d->size; i++)
            file << d->buffer[i] << ", ";

        file << std::endl;
    }
    virtual WorkerCallback * clone() override 
    {
        return new WorkerCallbackImpl();
    }
};

struct StartEndSharedEvent
{
    StartEndSharedEvent() : started(false), end(false) {}
    std::mutex  startEndLock;
    std::condition_variable startEndCheck;
    bool started;
    bool end;
    void waitForStart()
    {
        std::unique_lock<std::mutex> u(startEndLock);
        startEndCheck.wait(u, [this](){ return started; });
    }
    void waitForEnd(uint usec)
    {
        std::unique_lock<std::mutex> u(startEndLock);
        startEndCheck.wait_for(u, std::chrono::microseconds(usec), [this](){ return end; });
    }
};

class WorkerThread
{
    std::unique_ptr<WorkerCallback> cb;
    std::shared_ptr<ThreadSafeQueue<Data>> qu;
    StartEndSharedEvent & e;
public:
    WorkerThread(WorkerCallback * c, ThreadSafeQueue<Data> * q, StartEndSharedEvent &event) : cb(c), qu(q), e(event) {}
    void run()
    {
        LOG("Running thread");
        LOG("Waiting for data");

        e.waitForStart();

        LOG("Data Signal received Proceeding to process data");

        while(true)
        {
            if(!qu->empty())
            {
                cb->onData(qu->front());
                qu->pop();
            } 
            else
            {
                LOG("Queue empty, waiting for end signal for 500 us");
                e.waitForEnd(500);
                std::lock_guard<std::mutex> l(e.startEndLock);
                if(e.end)
                    break;
            }
        }
    } 
};


class MainThread
{
    std::thread * threads;
    WorkerThread ** workers;
    ThreadSafeQueue<Data> * queues;
    uint workersCount;
    
    StartEndSharedEvent startEndEvent;

    bool firstDataSent;
public:
    MainThread() : threads(nullptr), workers(nullptr), queues(nullptr), firstDataSent(false) {}
    bool init(uint threadCount, WorkerCallback * cb)
    {
        workersCount = threadCount;
        threads = new std::thread[threadCount];
        workers = new WorkerThread*[threadCount];
        queues = new ThreadSafeQueue<Data>[threadCount];
            
        for(uint i = 0; i < threadCount; i++)
        {
            workers[i] = new WorkerThread((i == 0) ? cb : cb->clone(), queues+i, startEndEvent);
            threads[i] = std::thread(&WorkerThread::run, *(workers + i));
        }
        return true; 
    } 

    void sendTo(uint threadId, Data * d)
    {
        assert(threadId < workersCount); 
        queues[threadId].push(d);

        if(!firstDataSent)
        {
            std::lock_guard<std::mutex> l(startEndEvent.startEndLock);
            startEndEvent.started = true;
            startEndEvent.startEndCheck.notify_all();
            firstDataSent = true;
        }
    }

    ~MainThread()
    {
        {
            std::lock_guard<std::mutex> l(startEndEvent.startEndLock);
            startEndEvent.end = true; 
            startEndEvent.startEndCheck.notify_all();
        }

        for(uint i =0; i < workersCount; i++)
        {
            threads[i].join();
        }
        delete [] threads;
        delete [] workers;
    }
};
