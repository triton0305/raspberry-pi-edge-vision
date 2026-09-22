#ifndef PREPROCESSOR_HPP
#define PREPROCESSOR_HPP

#include <opencv2/core.hpp>

class Preprocessor
{
public:
  Preprocessor(int input_width = 640, int input_height = 640);

  cv::Mat process(const cv::Mat& image) const;

  int inputWidth() const;
  int inputHeight() const;

private:
  int input_width_;
  int input_height_;
};

#endif // PREPROCESSOR_HPP