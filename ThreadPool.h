#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <thread>
#include <vector>
#include <mutex>

class ThreadPool
{
private:
    void doWork()
    {
        while (!st.stop_requested())
        {
            std::function<void()> job;
            {
                std::unique_lock lock(mtx);
                if (!cv.wait(lock, st, [&]
                             { return !q.empty(); }))
                {
                    return;
                }
                job = std::move(q.front());
                q.pop_front();
            }
            job();
        }
    }

    std::deque<std::function<void()>> q;
    std::vector<std::jthread> workers;
    std::mutex mtx;
    std::condition_variable_any cv;
    std::stop_token st;

public:
    ThreadPool(int n, std::stop_token &st) : st(st)
    {
        for (int i = 0; i < n; ++i)
        {
            workers.emplace_back([this]
                                 { doWork(); });
        }
    }

    void submit(std::function<void()> job)
    {
        {
            std::scoped_lock lock(mtx);
            q.push_back(std::move(job));
        }
        cv.notify_one();
    }
};

template <typename T>
class OrderedEmitter
{
private:
    std::mutex mtx;
    std::condition_variable_any cv;
    std::stop_token st;
    std::map<uint32_t, T> buffer;
    uint32_t nextSeq;
    std::atomic<bool> finished{false};

public:
    OrderedEmitter(std::stop_token &st) : st(st), nextSeq(0) {}

    void submitResult(uint64_t seq, T r)
    {
        {
            std::scoped_lock lock(mtx);
            buffer.emplace(seq, std::move(r));
        }
        cv.notify_one();
    }

    void emitResults(std::function<void(const T &)> consumer)
    {
        while (!finished)
        {
            std::unique_lock lock(mtx);
            if (!cv.wait(lock, st, [&]
                         { return buffer.count(nextSeq) > 0 || finished; }))
            {
                return;
            }

            while (true)
            {
                auto iter = buffer.find(nextSeq);
                if (iter == buffer.end())
                {
                    break;
                }
                consumer(iter->second);
                buffer.erase(iter);
                ++nextSeq;
            }
        }

        // Add a special case for our current task
        auto iter = buffer.find(-1);
        if (iter != buffer.end())
        {
            consumer(iter->second);
            buffer.erase(iter);
        }
    }

    void notifyComplete()
    {
        finished = true;
        cv.notify_one();
    }
};