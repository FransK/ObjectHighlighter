#ifndef OBJECT_HIGHLIGHTER
#define OBJECT_HIGHLIGHTER

#include "VideoProcessor.h"
#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>

class ObjectHighlighter : public VideoProcessor
{
public:
    struct Highlight
    {
        uint frame;
        cv::Rect box;
    };

    ObjectHighlighter() = default;
    void captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame);
    void objectSelection(cv::Mat &frame);
    void playVideo() override;
    void saveVideoWithHighlightsSerial(const std::string &outputPath, const std::string &format);
    void saveVideoWithHighlightsParallel(const std::string &outputPath, const std::string &format);

private:
    std::vector<Highlight> mHighlights; // TODO Further optimization: Add index into highlights for rewinding

    std::queue<cv::Mat> mInputFrames;
    std::queue<cv::Mat> mProcessedFrames;

    std::mutex mInputMutex;
    std::mutex mProcessedMutex;

    std::condition_variable mInputCv;
    std::condition_variable mProcessedCv;

    bool mReadingFinished = false;
    bool mProcessingFinished = false;

    void readFrames();
    void drawHighlights();
    void writeFrames(const std::string &outputPath, const std::string &format);
};

#endif