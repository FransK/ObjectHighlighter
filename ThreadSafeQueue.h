#ifndef THREAD_SAFE_QUEUE
#define THREAD_SAFE_QUEUE

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>

// Thread-safe queue with a maximum size and generation tracking
template <typename T>
class ThreadSafeQueue
{
private:
    std::condition_variable_any mNotFullCv, mNotEmptyCv;
    std::deque<T> mQueue;
    std::mutex mMutex;
    uint32_t mGeneration{0};
    uint32_t mMaxSize;

public:
    // Constructor with maximum queue size
    ThreadSafeQueue(uint32_t maxSize) : mMaxSize(maxSize) {}
    // Delete copy and move constructors and assignment operators
    ThreadSafeQueue(const ThreadSafeQueue &) = delete;
    ThreadSafeQueue operator=(const ThreadSafeQueue &) = delete;
    ThreadSafeQueue(ThreadSafeQueue &&) = delete;
    ThreadSafeQueue operator=(ThreadSafeQueue &&) = delete;

    // Push a new item into the queue, waiting if necessary
    // If the generation changes while waiting, the item is not added
    // If the stop token is triggered while waiting, the item is not added
    void push(T value, std::stop_token st)
    {
        std::unique_lock lock(mMutex);
        uint32_t currentGen = mGeneration;

        if (!mNotFullCv.wait(lock, st, [this]
                             { return mQueue.size() < mMaxSize; }))
        {
            // Woken by stop token
            return;
        }

        if (mGeneration != currentGen)
        {
            // Queue was cleared, don't add the item
            return;
        }

        // Add the item to the queue
        mQueue.push_back(std::move(value));
        lock.unlock();

        // Notify next waiting consumer
        mNotEmptyCv.notify_one();
    }

    // Clear the queue and increment the generation
    void clear()
    {
        {
            std::scoped_lock lock(mMutex);
            mQueue.clear();
            mGeneration += 1;
        }

        // Notify all waiting threads to recheck conditions
        mNotFullCv.notify_all();
        mNotEmptyCv.notify_all();
    }

    // Wait for and pop an item from the queue
    // If the generation changes while waiting, returns std::nullopt
    // If the stop token is triggered while waiting, returns std::nullopt
    std::optional<T> waitAndPop(std::stop_token st)
    {
        std::unique_lock lock(mMutex);
        uint32_t currentGen = mGeneration;

        if (!mNotEmptyCv.wait(lock, st, [this]
                              { return !mQueue.empty(); }))
        {
            // Woken by stop token
            return std::nullopt;
        }

        // Check if queue cleared while waiting
        if (mGeneration != currentGen || mQueue.empty())
        {
            return std::nullopt;
        }

        // Remove and return the front item
        T value = std::move(mQueue.front());
        mQueue.pop_front();
        lock.unlock();

        // Notify next waiting producer
        mNotFullCv.notify_one();

        // Return the item
        return value;
    }

    // Check if the queue is empty
    bool empty() const
    {
        std::scoped_lock lock(mMutex);
        return mQueue.empty();
    }
};

#endif