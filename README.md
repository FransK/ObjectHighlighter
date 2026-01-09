# Object Highlighter

A simple command line program that will allow users to highlight objects in a video (.mp4 supported) and then watch as the objects are tracked through the video.

### Building

The program can be built using either make or cmake.

Make will produce an executable named 'main' in the build directory; while Cmake can be run from the build directory itself to produce the executable named 'ObjectHighlighter'.


### Running

The program takes 3 arguments: A required video file and optionally an output file and format for video writing.


### Algorithm

The main program runs and creates an ObjectHighlighter object. Within the ObjectHighlighter, I create 4 main entities:
- 3 threads that run the main pipeline of the program:
 - A frame reader to get the next frame from the given video
 - A thread to update all the trackers
 - An output thread that either displays a video or saves+displays a video
- A ControlNode which maintains the state of the ObjectHighlighter and protects resources from multi-threaded race conditions with mutexes and atomics


### Performance

The ObjectHighlighter currently supports 16 threads for processing trackers as this is the most expensive portion of the pipeline (performance analyzed with std::chrono and Valgrind). These threads live in a threadpool to avoid spooling/teardown.


### Sample Video Highlighting

![Sample Highlighter](docs/object_highlighter.gif)
