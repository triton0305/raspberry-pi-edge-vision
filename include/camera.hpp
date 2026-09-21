#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

class Camera
{
public:
  Camera(int device_index = 0, int width = 640, int height = 480, int fps = 30);

  bool open();
  bool read(cv::Mat& frame);
  bool isOpened() const;
  void release();

private:
  int device_index_;
  int width_;
  int height_;
  int fps_;

  cv::VideoCapture capture_;
};

#endif // CAMERA_HPP