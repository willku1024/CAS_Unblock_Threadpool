#include <list>
#include <vector>
#include <thread>
#include <functional> // for std::function, std::bind
#include <assert.h>
#include <future>
#include <atomic>

using namespace std;

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

    void stop();
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
    
    std::condition_variable condition_;

    std::list<Task> taskQueue_;
    uint32_t threadsNum_;
    uint32_t queueSize_ = 0;
};

ThreadPool::ThreadPool(uint32_t threadNum, uint32_t queueSize)
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
    stop();
}

template <class Function, class... Args>
std::future<typename std::result_of<Function(Args...)>::type>
ThreadPool::addTask(Function &&fcn, Args &&... args)
{

    using retType = typename std::result_of<Function(Args...)>::type;
    using asyncTaskType = std::packaged_task<retType()>;

    asyncTaskType ptask(std::bind(fcn, args...));
    shared_ptr<asyncTaskType> t = std::make_shared<asyncTaskType>(std::move(ptask));
    auto ret = t->get_future();

    {
        std::lock_guard<std::mutex> lg(mutex_);
        
        taskQueue_.push_back([t]() { (*t)(); });
        
        condition_.notify_one();
    }


    return ret;
}

void ThreadPool::stop()
{
    if (!isRunning_.load(std::memory_order_consume))
    {
        return;
    }

    isRunning_.store(false, std::memory_order_release);
    
    condition_.notify_all();

    for (size_t i = 0; i != threads_.size(); ++i)
    {
        if (threads_[i].joinable())
        {
            threads_[i].join();
        }
    }
}

uint32_t ThreadPool::size()
{
    std::unique_lock<std::mutex> ulk(this->mutex_);
    uint32_t size = taskQueue_.size();
    
    return size;
}

ThreadPool::Task ThreadPool::take()
{
    Task task = NULL;
    
    std::unique_lock<std::mutex> ulk(this->mutex_);

    condition_.wait(ulk, [this] { 
        return !isRunning_.load(std::memory_order_consume) 
            || !this->taskQueue_.empty(); });
    

    if (!isRunning_.load(std::memory_order_consume))
    {
        ulk.unlock();
        return task;
    }

    assert(!taskQueue_.empty());
    task = taskQueue_.front();
    taskQueue_.pop_front();
    
    return task;
}

void *ThreadPool::threadFunc(void *arg)
{
       
    auto &&pool = this;
    while (pool->isRunning_)
    {
        ThreadPool::Task task = pool->take();
        if (!task)
        {
            break;
        }

        assert(task);
        task();
    }
    return 0;
}
