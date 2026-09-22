#include "preprocessor.hpp"

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
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

  const float scale = std::min(
    static_cast<float>(input_width_) / image.cols,
    static_cast<float>(input_height_) / image.rows);

  const int resized_width = static_cast<int>(std::round(image.cols * scale));
  const int resized_height = static_cast<int>(std::round(image.rows * scale));

  cv::Mat resized;
  cv::resize(image, resized, cv::Size(resized_width, resized_height));

  const int pad_left = (input_width_ - resized_width) / 2;
  const int pad_right = input_width_ - resized_width - pad_left;
  const int pad_top = (input_height_ - resized_height) / 2;
  const int pad_bottom = input_height_ - resized_height - pad_top;

  cv::Mat letterboxed;
  cv::copyMakeBorder(
    resized, letterboxed,
    pad_top, pad_bottom,
    pad_left, pad_right,
    cv::BORDER_CONSTANT,
    cv::Scalar(114, 114, 114));

  return cv::dnn::blobFromImage(
    letterboxed, 1.0 / 255.0,
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