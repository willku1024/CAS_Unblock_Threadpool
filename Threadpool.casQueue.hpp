#include <deque>
#include <thread>
#include <functional> 
#include <assert.h>
#include <future>
#include <atomic>

#include "RingQueue.hpp"

using namespace std;
using namespace rq;
class ThreadPool
{
public:
    using Task = std::function<void()>;

public:
    ThreadPool(uint32_t threadNum):ThreadPool(threadNum, threadNum){};
    ThreadPool(uint32_t threadNum, uint32_t queueSize);
    ~ThreadPool();

public:
    template <class Function, class... Args>
    std::future<typename std::result_of<Function(Args...)>::type> addTask(Function &&, Args &&...);

    uint32_t size();
    Task take();

private:
    
    void *threadFunc(void* arg);

private:
    ThreadPool &operator=(const ThreadPool &);
    ThreadPool(const ThreadPool &);

private:
    
    std::atomic<bool> isRunning_;
    
    std::vector<std::thread> threads_;
    
    std::mutex mutex_;
    
    std::condition_variable emptycond_;
    std::condition_variable fullcond_;

    
    rq::LockFreeRingQueue<Task> taskQueue_;
    uint32_t threadsNum_;
    uint32_t queueSize_;
};

ThreadPool::ThreadPool(uint32_t threadNum, uint32_t queueSize):taskQueue_(queueSize_)
{
    isRunning_.store(true, std::memory_order_release);
    threadsNum_ = threadNum;
   
    for (uint32_t i = 0; i < threadsNum_; i++)
    {
        threads_.push_back(thread(&ThreadPool::threadFunc, this, nullptr));     
    }
}

ThreadPool::~ThreadPool()
{
    if (!isRunning_.load(std::memory_order_consume))
    {
        return;
    }

    isRunning_.store(false, std::memory_order_release);
    
    emptycond_.notify_all();

    for (size_t i = 0; i != threads_.size(); ++i)
    {
        if (threads_[i].joinable())
        {
            threads_[i].join();
        }
    }
}

template <class Function, class... Args>
std::future<typename std::result_of<Function(Args...)>::type>
ThreadPool::addTask(Function &&fcn, Args &&... args)
{

    using retType = typename std::result_of<Function(Args...)>::type;
    
    {
        std::unique_lock<std::mutex> ulk(this->mutex_);


            fullcond_.wait(ulk, [this] { 
                            return !isRunning_.load(std::memory_order_consume) 
                            || (taskQueue_.size() != taskQueue_.capicity()); });
                                 
    
        if (!isRunning_.load(std::memory_order_consume))
        {
            ulk.unlock();
            return std::future<retType>();
        }
    }

    using asyncTaskType = std::packaged_task<retType()>;
    auto t = std::make_shared<asyncTaskType>(std::bind(fcn, args...));

    
        taskQueue_.enQueue( [t]() { (*t)(); });  
        emptycond_.notify_one();
    

    return t->get_future();

}

uint32_t ThreadPool::size()
{
    std::unique_lock<std::mutex> ulk(this->mutex_);
    uint32_t size = taskQueue_.size();
    return size;
}

ThreadPool::Task ThreadPool::take()
{
    Task task;

    {
       std::unique_lock<std::mutex> ulk(this->mutex_);
        
  
        emptycond_.wait(ulk, [this] { 
            return !isRunning_.load(std::memory_order_consume) 
            || !this->taskQueue_.empty(); });
     

        if (!isRunning_.load(std::memory_order_consume))
        {
            ulk.unlock();
            return task;
        }

    assert(!taskQueue_.empty());
    taskQueue_.deQueue(task);
    }
    
    
    
    return task;
}

void *ThreadPool::threadFunc(void *arg)
{
       
    auto &&pool = this;
    while (pool->isRunning_)
    {
        ThreadPool::Task &&task = pool->take();
        if (!task)
        {
            break;
        }

        
        assert(task);

 
        fullcond_.notify_all();
      
        task();
        
    }
    return 0;
}
