#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

class WebcamRecorder {
private:
    cv::VideoCapture cap;
    cv::VideoWriter writer;
    std::atomic<bool> recording;
    std::atomic<bool> stopRequested;
    std::thread recordingThread;
    std::string outputFilename;
    
public:
    WebcamRecorder() : recording(false), stopRequested(false) {}
    
    ~WebcamRecorder() {
        stopRecording();
    }
    
    bool startRecording(const std::string& filename = "output.avi", 
                       int cameraIndex = 0, 
                       double fps = 30.0,
                       int frameWidth = 640,
                       int frameHeight = 480) {
        
        if (recording.load()) {
            std::cout << "Recording is already in progress!" << std::endl;
            return false;
        }
        
        // Open camera
        cap.open(cameraIndex);
        if (!cap.isOpened()) {
            std::cout << "Error: Cannot open camera " << cameraIndex << std::endl;
            return false;
        }
        
        cap.set(cv::CAP_PROP_FRAME_WIDTH, frameWidth);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, frameHeight);
        cap.set(cv::CAP_PROP_FPS, fps);
        
        int actualWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int actualHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        
        outputFilename = filename;
        int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        writer.open(outputFilename, fourcc, fps, cv::Size(actualWidth, actualHeight));
        
        if (!writer.isOpened()) {
            std::cout << "Error: Cannot open video writer for " << filename << std::endl;
            cap.release();
            return false;
        }
        
        // Start recording in separate thread
        stopRequested.store(false);
        recording.store(true);
        recordingThread = std::thread(&WebcamRecorder::recordingLoop, this);
        
        std::cout << "Recording started: " << filename << std::endl;
        std::cout << "Resolution: " << actualWidth << "x" << actualHeight << std::endl;
        std::cout << "Press 'q' in the video window or call stopRecording() to stop." << std::endl;
        
        return true;
    }
    
    void stopRecording() {
        if (!recording.load()) {
            return;
        }
        
        stopRequested.store(true);
        
        if (recordingThread.joinable()) {
            recordingThread.join();
        }
        
        recording.store(false);
        
        // Release resources
        if (writer.isOpened()) {
            writer.release();
        }
        if (cap.isOpened()) {
            cap.release();
        }
        
        cv::destroyAllWindows();
        std::cout << "Recording stopped. Video saved as: " << outputFilename << std::endl;
    }
    
    bool isRecording() const {
        return recording.load();
    }
    
private:
    void recordingLoop() {
        cv::Mat frame;
        
        while (!stopRequested.load()) {
            cap >> frame;
            
            if (frame.empty()) {
                std::cout << "Warning: Empty frame captured" << std::endl;
                continue;
            }
            
            // Write frame to video file
            writer << frame;
            cv::imshow("Recording - Press 'q' to stop", frame);
            char key = cv::waitKey(1) & 0xFF;
            if (key == 'q' || key == 'Q') {
                stopRequested.store(true);
                break;
            }
        }
    }
};
