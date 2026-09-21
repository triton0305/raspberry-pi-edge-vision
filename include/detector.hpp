#ifndef DETECTOR_HPP
#define DETECTOR_HPP

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

#include <string>
#include <vector>

class Detector
{
public:
  explicit Detector(const std::string& model_path);

  std::vector<cv::Mat> infer(const cv::Mat& input_blob);
  bool isLoaded() const;

private:
  cv::dnn::Net net_;
};

#endif // DETECTOR_HPP