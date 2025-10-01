#include "ObjectHighlighter.h"
#include "VideoProcessor.h"

#include <iostream>

#include "opencv2/core.hpp"

const char *keys =
    "{help h usage ?  |             | print this message            }"
    "{@video          |             | video file path (required)    }"
    "{output o        | output.mp4  | output video file path        }"
    "{format f        | mp4v        | video format                  }";

int main(int argc, char *argv[])
{
    cv::CommandLineParser parser(argc, argv, keys);
    parser.about("Object Highlighter v1.0");

    // If help is requested or video path not provided, show help and exit
    if (parser.has("help") || !parser.has("@video"))
    {
        parser.printMessage();
        return 0;
    }

    // Check if the parser is correctly initialized
    if (!parser.check())
    {
        parser.printErrors();
        return 1;
    }

    // Get the video path from command line arguments
    std::string videoPath = parser.get<std::string>("@video");

    ObjectHighlighter vp;
    if (!vp.loadVideo(videoPath))
    {
        std::cerr << "Error: Could not open video file: " << videoPath << std::endl;
        return 1;
    }

    vp.displayInfo();

    std::string outputPath = parser.get<std::string>("output");
    std::string format = parser.get<std::string>("format");

    vp.writerSettings(outputPath, format);

    vp.playVideo();

    return 0;
}