#include "ObjectHighlighter.h"
#include "VideoProcessor.h"

#include <iostream>

#include "opencv2/core.hpp"

// Command line argument keys
const char *keys =
    "{help h usage ?  |             | print this message            }"
    "{@video          |             | video file path (required)    }"
    "{output o        | output.mp4  | output video file path        }"
    "{format f        | mp4v        | video format                  }";

int main(int argc, char *argv[])
{
    // Parse command line arguments
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

    // Get output path and format from command line arguments
    std::string outputPath = parser.get<std::string>("output");
    std::string format = parser.get<std::string>("format");

    // Create ObjectHighlighter instance and load the video
    ObjectHighlighter vp;
    if (!vp.loadVideo(videoPath))
    {
        std::cerr << "Error: Could not open video file: " << videoPath << std::endl;
        return 1;
    }

    // Display video information
    vp.displayInfo();

    // Set writer settings
    vp.writerSettings(outputPath, format);

    // Start video playback and processing
    vp.playVideo();

    return 0;
}