#ifndef OBJECT_HIGHLIGHTER
#define OBJECT_HIGHLIGHTER

#include "VideoProcessor.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>

#include "opencv2/tracking.hpp"

class ObjectHighlighter : public VideoProcessor
{
public:
    struct Highlight
    {
        uint frame;
        cv::Rect box;
    };

    struct ObjectTracker
    {
        cv::Ptr<cv::Tracker> tracker;
        cv::Rect box;
    };

    ObjectHighlighter() = default;
    void captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame);
    void objectSelection(cv::Mat &frame);
    void playVideo() override;
    void saveVideoWithHighlights(const std::string &outputPath, const std::string &format);

private:
    struct PlaybackState
    {
        std::queue<cv::Mat> processorFrames{};
        std::queue<cv::Mat> writerFrames{};

        std::mutex preProcessorMutex;
        std::mutex processorMutex;
        std::mutex writerMutex;

        std::condition_variable preProcessorCv;
        std::condition_variable processorCv;
        std::condition_variable writerCv;

        std::atomic<uint64_t> preProcessorGen{0};
        std::atomic<uint64_t> processorGen{0};

        std::atomic<bool> processingFinished{false};
        std::atomic<bool> readingFinished{false};
        std::atomic<bool> shuttingDown{false};

        void requestShutdown()
        {
            shuttingDown.store(true, std::memory_order_release);
            preProcessorCv.notify_all();
            processorCv.notify_all();
            writerCv.notify_all();
        }
    };

    struct SaveVideoState
    {
        std::queue<cv::Mat> inputFrames{};
        std::queue<cv::Mat> processedFrames{};

        std::mutex inputMutex;
        std::mutex processedMutex;

        std::condition_variable inputCv;
        std::condition_variable processedCv;

        std::atomic<bool> readingFinished{false};
        std::atomic<bool> processingFinished{false};
    };

    std::vector<Highlight> mHighlights; // TODO Further optimization: Add index into highlights for rewinding
    std::vector<ObjectTracker> mTrackers;

    void captureFrames(PlaybackState &state);
    void updateTrackers(PlaybackState &state);
    void drawFrames(PlaybackState &state);

    void readFrames(SaveVideoState &state);
    void drawHighlights(SaveVideoState &state);
    void writeFrames(const std::string &outputPath, const std::string &format, SaveVideoState &state);

    bool handlePlaybackInput(int key, cv::Mat &frame, uint framec, std::vector<Highlight>::iterator &hlIter);
    void drawHighlightsOnFrame(cv::Mat &frame, uint framec, std::vector<Highlight>::iterator &hlIter);
    void trackOnFrame(cv::Mat &frame);
};

#endif