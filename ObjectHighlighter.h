#ifndef OBJECT_HIGHLIGHTER
#define OBJECT_HIGHLIGHTER

#include "ThreadSafeQ.h"
#include "VideoProcessor.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>
#include <vector>

#include "opencv2/tracking.hpp"

static const std::string sWindowTitle{"Video"};

class ObjectHighlighter : public VideoProcessor
{
public:
    struct Frame
    {
        int idx;
        cv::Mat frame;
    };

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
    void selectObjects(Frame &frame);
    void saveVideoWithHighlights(const std::string &outputPath, const std::string &format);
    void captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame);

private:
    struct PlaybackState
    {
        std::atomic<uint64_t> processorGen{0};
        std::atomic<bool> trackerInvalid{false};

        ThreadSafeQ<Frame> processorQueue{8};
        ThreadSafeQ<Frame> writerQueue{8};

        std::mutex capMutex;
        std::mutex trackersMutex;

        std::stop_source stopSource;
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

    bool handlePlaybackInput(int key, PlaybackState &state, Frame &myFrame);
    void drawHighlightsOnFrame(cv::Mat &frame, uint framec, std::vector<Highlight>::iterator &hlIter);
    void trackOnFrame(Frame &frame);
};

#endif