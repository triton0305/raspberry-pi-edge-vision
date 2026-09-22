#include "vision/camera.hpp"

#include <opencv2/videoio.hpp>

#include <iostream>

Camera::Camera(int device_index, int width, int height, int fps)
  : device_index_(device_index), width_(width), height_(height), fps_(fps)
{
}

bool Camera::open()
{
  if (!capture_.open(device_index_, cv::CAP_V4L2))
  {
    std::cerr << "Camera: failed to open device " << device_index_ << '\n';
    return false;
  }

  capture_.set(cv::CAP_PROP_FRAME_WIDTH, width_);
  capture_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
  capture_.set(cv::CAP_PROP_FPS, fps_);

  const int actual_width = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_WIDTH));
  const int actual_height = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_HEIGHT));
  const double actual_fps = capture_.get(cv::CAP_PROP_FPS);

  std::cout << "Camera: "
            << actual_width << 'x' << actual_height
            << " @ " << actual_fps << " FPS\n";

  return true;
}

bool Camera::read(cv::Mat& frame)
{
  if (!capture_.isOpened())
  {
    std::cerr << "Camera: device is not opened\n";
    return false;
  }

  if (!capture_.read(frame) || frame.empty())
  {
    std::cerr << "Camera: failed to read frame\n";
    return false;
  }

  return true;
}

bool Camera::isOpened() const
{
  return capture_.isOpened();
}

void Camera::release()
{
  if (capture_.isOpened())
    capture_.release();
}