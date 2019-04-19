#include <stdexcept>
#include <atomic>
#include <iostream>
using namespace std;

#define CAS(o, e, d) atomic_compare_exchange_weak(o, e, d)

/*
   基于无锁的队列特点：
   1、CAS操作是原子的,线程并行执行push/pop不会导致死锁
   2、多生产者同时向队列push数据的时候不会将数据写入到同一个位置,产生数据覆盖
   3、多消费者同时执行pop不会导致一个元素被出列多于1次
   4、线程不能将数据push进已经满的队列中,不能从空的队列中pop元素
   5、push和pop都没有ABA问题

   但是,虽然这个队列是线程安全的,但是在多生产者线程的环境下它的性能还是不如阻塞队列.
   因此,在符合下述条件的情况下可以考虑使用这个队列来代替阻塞队列:
   1、只有一个生产者线程（比较适用于线程池模型）
   2、只有一个频繁操作队列的生产者,但偶尔会有其它生产者向队列push数据
 */

namespace rq
{
    template <typename T>
    class LockFreeRingQueue
    {
    public:
        explicit LockFreeRingQueue(unsigned int size);
        virtual ~LockFreeRingQueue() {}

        // 入队
        bool enQueue(T &t);
        bool enQueue(T &&t);

        // 出队
        bool deQueue(T &t);
        bool deQueue(T &&t) = delete;

        // 环形队列最大可容纳
        uint32_t capicity();

    #ifndef ISEMP

        // 队空
        bool empty(void);

        // 队列此时长度
        uint32_t size();
    #endif // ifndef ISEMP

    private:
        // 判断size是够为2的幂
        inline bool isPowerOfTwo(uint32_t num)
        {
            return num != 0 && (num & (num - 1)) == 0;
        }

        inline uint32_t ceilPowerOfTwo(uint32_t num)
        {
            num |= (num >> 1);
            num |= (num >> 2);
            num |= (num >> 4);
            num |= (num >> 8);
            num |= (num >> 16);
            return num - (num >> 1);
        }

        inline uint32_t roundupPowerOfTwo(uint32_t num)
        {
            return ceilPowerOfTwo((num - 1) << 1);
        }

        inline uint32_t indexOfQueue(uint32_t index)
        {
            // 当size为2的幂时，index % size 可以转化为 index & (size – 1)，详见kfifo
            return index & (this->_size - 1);
        }

    private:
        uint32_t _size;
        atomic_int32_t _length;

        /*
        writeIndex:新元素入列时存放位置在数组中的下标
        readIndex:下一个出列元素在数组中的下标
        maxReadIndex:最后一个已经完成入列操作的元素在数组中的下标.
                        如果它的值跟writeIndex不一致,表明有写请求尚未完成.
                        这意味着,有写请求成功申请了空间但数据还没完全写进队列.
                        所以如果有线程要读取,必须要等到写线程将数完全据写入到队列之后.
        */
        using COUNTER = atomic<uint32_t>;
        COUNTER _readIndex;
        COUNTER _writeIndex;
        COUNTER _lastWriteIndex;

        std::unique_ptr<T[]> _queue;
    };

    template <typename T>
    LockFreeRingQueue<T>::LockFreeRingQueue(unsigned int size) : _length(0),
                                                                _readIndex(0),
                                                                _writeIndex(0),
                                                                _lastWriteIndex(0)
    {
        if (size <= 0)
            throw std::out_of_range("queue size is invalid");
        else if (1 == size)
            this->_size = 2;
        else
            this->_size = isPowerOfTwo(size) ? size : roundupPowerOfTwo(size);

        _queue = std::move(std::unique_ptr<T[]>(new T[this->_size]));
    }

    template <typename T>
    bool LockFreeRingQueue<T>::enQueue(T &data)
    {
        uint32_t currentReadIndex;
        uint32_t currentWriteIndex;

        do
        {
            currentReadIndex = this->_readIndex;
            currentWriteIndex = this->_writeIndex;

            if (indexOfQueue(currentWriteIndex + 1) ==
                indexOfQueue(currentReadIndex))
            {
                return false;
            }

        } while (!CAS(&this->_writeIndex,
                    &currentWriteIndex,
                    (currentWriteIndex + 1)));

        this->_queue[indexOfQueue(currentWriteIndex)] = data;

        while (!CAS(&this->_lastWriteIndex, &currentWriteIndex,
                    currentWriteIndex + 1))
        {
            // this is a good place to yield the thread in case there are more
            // software threads than hardware processors and you have more
            // than 1 producer thread
            // have a look at sched_yield (POSIX.1b)
            sched_yield();
        }


    #ifndef ISEMP
        atomic_fetch_add(&this->_length, 1);
    #endif // ifndef ISEMP

        return true;
    }

    template <typename T>
    bool LockFreeRingQueue<T>::enQueue(T &&data)
    {
         return enQueue(data);
    }

    template <typename T>
    bool LockFreeRingQueue<T>::deQueue(T &data)
    {
        uint32_t currentReadIndex;
        uint32_t currentLastWriteIndex;

        do
        {
            currentReadIndex = this->_readIndex;
            currentLastWriteIndex = this->_lastWriteIndex;

            if (indexOfQueue(currentLastWriteIndex) ==
                indexOfQueue(currentReadIndex))
            {
                // the queue is empty or
                // a producer thread has allocate space in the queue but is
                // waiting to commit the data into it
                break;
            }

            // retrieve the data from the queue
            data = this->_queue[indexOfQueue(currentReadIndex)];

            // try to perfrom now the CAS operation on the read index. If we succeed
            // a_data already contains what m_readIndex pointed to before we
            // increased it
            if (CAS(&this->_readIndex, &currentReadIndex, currentReadIndex + 1))
            {
    #ifndef ISEMP
                atomic_fetch_sub(&this->_length, 1);
    #endif // ifndef ISEMP
                return true;
            }
        } while (1);

        return false;
    }

    template <typename T>
    uint32_t LockFreeRingQueue<T>::capicity()
    {
        return this->_size-1;
    }
  

    #ifndef ISEMP
    template <typename T>
    bool LockFreeRingQueue<T>::empty()
    {
        return 0 == this->_length;
    }

    template <typename T>
    uint32_t LockFreeRingQueue<T>::size()
    {
        return this->_length;
    }
    #endif // ifndef ISEMP
} // namespace rq
