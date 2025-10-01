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
    void doWork(std::stop_token st)
    {
        while (!st.stop_requested())
        {
            std::function<void()> job;
            {
                std::unique_lock lock(mWorkMutex);
                if (!mWorkCv.wait(lock, st, [this]
                                  { return !mWorkQueue.empty(); }))
                {
                    return;
                }

                job = std::move(mWorkQueue.front());
                mWorkQueue.pop_front();
            }

            job();

            if (mPendingJobs.fetch_sub(1) == 1)
            {
                std::scoped_lock lock(mCompletionMutex);
                mCompletionCv.notify_all();
            }
        }
    }

    std::deque<std::function<void()>> mWorkQueue;
    std::vector<std::jthread> mWorkers;
    std::mutex mWorkMutex;
    std::condition_variable_any mWorkCv;

    // Members for waitAll()
    std::atomic<int> mPendingJobs{0};
    std::mutex mCompletionMutex;
    std::condition_variable mCompletionCv;

public:
    ThreadPool(int n, std::stop_token st)
    {
        for (int i = 0; i < n; ++i)
        {
            mWorkers.emplace_back(&ThreadPool::doWork, this, st);
        }
    }

    ~ThreadPool() = default;

    void submit(std::function<void()> job)
    {
        mPendingJobs.fetch_add(1);
        {
            std::scoped_lock lock(mWorkMutex);
            mWorkQueue.push_back(std::move(job));
        }
        mWorkCv.notify_one();
    }

    template <typename Rep, typename Period>
    bool waitAll(const std::chrono::duration<Rep, Period> &timeout)
    {
        std::unique_lock lock(mCompletionMutex);
        return mCompletionCv.wait_for(lock, timeout, [this]
                                      { return mPendingJobs.load() == 0; });
    }
};