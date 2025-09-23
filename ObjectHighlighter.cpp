#include <iostream>

#include "opencv2/features2d.hpp"
#include "opencv2/imgproc.hpp"

#include "ObjectHighlighter.h"

using std::cout;
using std::endl;

void ObjectHighlighter::objectSelection(cv::Mat &frame)
{
    if (!mCap.isOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    std::vector<cv::Rect> boundingBoxes;
    cv::selectROIs("Video", frame, boundingBoxes);

    for (auto bbox : boundingBoxes)
    {
        Highlight hl;
        hl.frame = static_cast<int>(mCap.get(cv::CAP_PROP_POS_FRAMES));
        hl.box = bbox;
        mHighlights.push_back(hl);
        cout << "Added highlight: " << hl.frame << endl;
    }
}

void ObjectHighlighter::playVideo()
{
    if (!mCap.isOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    cv::Mat frame;
    uint framec = 0;

    std::vector<Highlight>::iterator hlIter;
    bool anyHighlights = !mHighlights.empty();
    if (anyHighlights)
    {
        hlIter = mHighlights.begin();
    }

    while (mCap.read(frame))
    {
        if (anyHighlights && hlIter->frame <= framec)
        {
            while (hlIter->frame < framec)
            {
                ++hlIter;
            }

            while (hlIter->frame == framec)
            {
                cout << "Drawing rectangle on frame: " << framec << endl;
                cv::rectangle(frame, hlIter->box, cv::Scalar(0, 255, 0));
                ++hlIter;
            }
        }

        cv::imshow("Video", frame);

        int key = cv::waitKey(35);

        if (key == 'q')
        {
            break;
        }
        else if (key == 'p')
        {
            objectSelection(frame);
            anyHighlights = !mHighlights.empty();
            if (anyHighlights)
            {
                hlIter = mHighlights.begin();
            }
        }
        else if (key == 'r')
        {
            rewindVideo(-1);
            framec = 0;
            if (anyHighlights)
            {
                hlIter = mHighlights.begin();
            }
        }
        else if (key == 'z')
        {
            rewindVideo(290);
            framec = static_cast<int>(mCap.get(cv::CAP_PROP_POS_FRAMES));
            if (anyHighlights)
            {
                hlIter = mHighlights.begin();
            }
        }

        ++framec;
    }

    cv::destroyAllWindows();
}

void ObjectHighlighter::saveVideoWithHighlights(const std::string &outputPath, const std::string &format)
{
}

void ObjectHighlighter::captureFrameWithHighlights(const std::string &outputPath, const uint &nframe)
{
}
