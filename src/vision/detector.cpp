#include "vision/detector.hpp"

#include <stdexcept>

Detector::Detector(const std::string& model_path)
{
  net_ = cv::dnn::readNetFromONNX(model_path);

  if (net_.empty())
  {
    throw std::runtime_error("Failed to load ONNX model: " + model_path);
  }

  net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
  net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
}

std::vector<cv::Mat> Detector::infer(const cv::Mat& input_blob)
{
  net_.setInput(input_blob);

  std::vector<cv::Mat> outputs;
  std::vector<std::string> output_names = net_.getUnconnectedOutLayersNames();

  net_.forward(outputs, output_names);

  return outputs;
}

bool Detector::isLoaded() const
{
  return !net_.empty();
}