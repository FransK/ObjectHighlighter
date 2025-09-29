#ifndef THREAD_SAFE_Q
#define THREAD_SAFE_Q

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>

template <typename T>
class ThreadSafeQ
{
private:
    std::condition_variable_any notFullCv, notEmptyCv;
    std::deque<T> q;
    std::mutex mtx;
    std::atomic<uint32_t> generation{0};
    uint32_t maxSize;

public:
    ThreadSafeQ(uint32_t maxSize) : maxSize(maxSize) {}
    ThreadSafeQ(const ThreadSafeQ &) = delete;
    ThreadSafeQ operator=(const ThreadSafeQ &) = delete;
    ThreadSafeQ(ThreadSafeQ &&) = delete;
    ThreadSafeQ operator=(ThreadSafeQ &&) = delete;

    void push(T value, std::stop_token st)
    {
        std::unique_lock lock(mtx);
        uint32_t currentGen = generation.load();

        if (!notFullCv.wait(lock, st, [this]
                            { return q.size() < maxSize; }))
        {
            // Woken by stop token
            return;
        }

        if (generation.load() != currentGen)
        {
            // Queue was cleared, don't add the item
            return;
        }

        q.push_back(std::move(value));
        lock.unlock();
        notEmptyCv.notify_one();
    }

    void clear()
    {
        {
            std::scoped_lock lock(mtx);
            q.clear();
            generation.fetch_add(1);
        }
        notFullCv.notify_all();
        notEmptyCv.notify_all();
    }

    std::optional<T> waitAndPop(std::stop_token st)
    {
        std::unique_lock lock(mtx);
        uint32_t currentGen = generation.load();

        if (!notEmptyCv.wait(lock, st, [this]
                             { return !q.empty(); }))
        {
            // Woken by stop token
            return std::nullopt;
        }

        // Check if queue cleared while waiting
        if (generation.load() != currentGen || q.empty())
        {
            return std::nullopt;
        }

        T value = std::move(q.front());
        q.pop_front();
        lock.unlock();
        notFullCv.notify_one();
        return value;
    }

    bool empty() const
    {
        std::scoped_lock lock(mtx);
        return q.empty();
    }
};

#endif