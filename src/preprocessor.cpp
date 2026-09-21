#include "preprocessor.hpp"

#include <opencv2/dnn.hpp>

#include <iostream>

Preprocessor::Preprocessor(int input_width, int input_height)
  : input_width_(input_width), input_height_(input_height)
{
}

cv::Mat Preprocessor::process(const cv::Mat& image) const
{
  if (image.empty())
  {
    std::cerr << "Preprocessor: empty image\n";
    return {};
  }

  return cv::dnn::blobFromImage(
    image, 1.0 / 255.0,
    cv::Size(input_width_, input_height_),
    cv::Scalar(), true, false);
}

int Preprocessor::inputWidth() const
{
  return input_width_;
}

int Preprocessor::inputHeight() const
{
  return input_height_;
}