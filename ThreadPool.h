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
    // Worker thread function
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
                // Unlock before executing the job
                lock.unlock();
            }

            // Execute the job
            job();

            // Notify waitAll() if this was the last pending job
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
    // Constructor with number of threads and stop token
    ThreadPool(int n, std::stop_token st)
    {
        for (int i = 0; i < n; ++i)
        {
            mWorkers.emplace_back(&ThreadPool::doWork, this, st);
        }
    }
    // Default destructor
    ~ThreadPool() = default;
    // Delete copy and move constructors and assignment operators
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;

    // Submit a new job to the thread pool
    void submit(std::function<void()> job)
    {
        mPendingJobs.fetch_add(1);
        {
            std::scoped_lock lock(mWorkMutex);
            mWorkQueue.push_back(std::move(job));
        }
        mWorkCv.notify_one();
    }

    // Wait until all submitted jobs are completed or timeout occurs
    // Returns true if all jobs completed, false if timeout occurred
    template <typename Rep, typename Period>
    bool waitAll(const std::chrono::duration<Rep, Period> &timeout)
    {
        std::unique_lock lock(mCompletionMutex);
        return mCompletionCv.wait_for(lock, timeout, [this]
                                      { return mPendingJobs.load() == 0; });
    }
};