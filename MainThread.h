#include <mutex>
#include <thread>
#include <queue>
#include <cassert>

#include <iostream>
#include <fstream>
#include <string>

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

class WorkerThread
{
    std::unique_ptr<WorkerCallback> cb;
    std::shared_ptr<ThreadSafeQueue<Data>> qu;
public:
    WorkerThread(WorkerCallback * c, ThreadSafeQueue<Data> * q) : cb(c), qu(q) {}
    void run()
    {
        std::cout << "Runing thread : "<< std::this_thread::get_id() << std::endl; 
        while(true)
        {
            if(!qu->empty())
            {
                cb->onData(qu->front());
                qu->pop();
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
public:
    MainThread() : threads(nullptr), workers(nullptr), queues(nullptr) {}
    bool init(uint threadCount, WorkerCallback * cb)
    {
        workersCount = threadCount;
        threads = new std::thread[threadCount];
        workers = new WorkerThread*[threadCount];
        queues = new ThreadSafeQueue<Data>[threadCount];
            
        for(uint i = 0; i < threadCount; i++)
        {
            workers[i] = new WorkerThread((i == 0) ? cb : cb->clone(), queues+i);
            threads[i] = std::thread(&WorkerThread::run, *(workers + i));
        }
        return true; 
    } 

    void sendTo(uint threadId, Data * d)
    {
        assert(threadId < workersCount); 
        queues[threadId].push(d);
    }

    ~MainThread()
    {
        for(uint i =0; i < workersCount; i++)
        {
            threads[i].join();
        }
        delete [] threads;
        delete [] workers;
    }
};
