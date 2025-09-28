#ifndef OBJECT_HIGHLIGHTER
#define OBJECT_HIGHLIGHTER

#include "VideoProcessor.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>
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
    void playVideo() override;
    void selectObjects(cv::Mat &frame);
    void saveVideoWithHighlights(const std::string &outputPath, const std::string &format);
    void captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame);

private:
    struct PlaybackState
    {
        std::atomic<uint64_t> processorGen{0};
        std::atomic<uint64_t> writerGen{0};

        std::queue<cv::Mat> processorQueue{};
        std::queue<cv::Mat> writerQueue{};

        std::mutex preProcessorMutex;
        std::mutex processorMutex;
        std::mutex writerMutex;

        std::condition_variable_any preProcessorCv;
        std::condition_variable_any processorCv;
        std::condition_variable_any writerCv;

        std::atomic<bool> preProcessingFinished{false};
        std::atomic<bool> processingFinished{false};

        std::stop_source stopSource;

        void requestShutdown()
        {
            stopSource.request_stop();
            preProcessorCv.notify_all();
            processorCv.notify_all();
            writerCv.notify_all();
        }
    };

    struct SaveVideoState
    {
        std::queue<cv::Mat> processorQueue{};
        std::queue<cv::Mat> writerQueue{};

        std::mutex processorMutex;
        std::mutex writerMutex;

        std::condition_variable processorCv;
        std::condition_variable writerCv;

        std::atomic<bool> preProcessingFinished{false};
        std::atomic<bool> processingFinished{false};
    };

    std::vector<Highlight> mHighlights;   // TODO Further optimization: Add index into highlights for rewinding
    std::vector<ObjectTracker> mTrackers; // Other optimizations: Have trackers running in background

    // Playback pipeline
    void captureFrames(PlaybackState &state);
    void updateTrackers(PlaybackState &state);
    void drawFrames(PlaybackState &state);

    // VideoWriter pipeline
    void readFrames(SaveVideoState &state);
    void drawHighlights(SaveVideoState &state);
    void writeFrames(const std::string &outputPath, const std::string &format, SaveVideoState &state);

    bool handlePlaybackInput(int key, PlaybackState &state, cv::Mat &frame);
    bool handlePlaybackInput(int key, cv::Mat &frame, uint framec, std::vector<Highlight>::iterator &hlIter);
    void drawHighlightsOnFrame(cv::Mat &frame, uint framec, std::vector<Highlight>::iterator &hlIter);
    void trackOnFrame(cv::Mat &frame);
};

#endif