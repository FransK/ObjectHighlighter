#ifndef THREAD_SAFE_Q
#define THREAD_SAFE_Q

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>

template <typename T>
class ThreadSafeQ
{
private:
    std::condition_variable_any cv;
    std::deque<T> q;
    std::mutex mtx;

public:
    ThreadSafeQ() = default;
    ThreadSafeQ(const ThreadSafeQ &) = delete;
    ThreadSafeQ operator=(const ThreadSafeQ &) = delete;

    void push(T value)
    {
        {
            std::scoped_lock lock(mtx);
            q.push_back(std::move(value));
        }
        cv.notify_one();
    }

    void clear()
    {
        {
            std::scoped_lock lock(mtx);
            q.clear();
        }
        cv.notify_one();
    }

    std::optional<T> waitAndPop(std::stop_token st)
    {
        std::unique_lock lock(mtx);
        if (!cv.wait(lock, st, [this]
                     { return !q.empty(); }))
        {
            // Woken by stop token
            return std::nullopt;
        }
        T value = std::move(q.front());
        q.pop_front();
        return value;
    }

    bool empty() const
    {
        std::scoped_lock lock(mtx);
        return q.empty();
    }
};

#endif