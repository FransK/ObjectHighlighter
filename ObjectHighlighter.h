#ifndef OBJECT_HIGHLIGHTER
#define OBJECT_HIGHLIGHTER

#include "VideoProcessor.h"
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
    void objectSelection(cv::Mat &frame);
    void playVideo() override;
    void saveVideoWithHighlights(const std::string &outputPath, const std::string &format);
    void captureFrameWithHighlights(const std::string &outputPath, const uint &nframe);

private:
    std::vector<Highlight> mHighlights;
};

#endif