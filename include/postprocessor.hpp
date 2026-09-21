#ifndef POSTPROCESSOR_HPP
#define POSTPROCESSOR_HPP

#include "detection_result.hpp"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

class PostProcessor
{
public:
  PostProcessor(float conf_threshold = 0.25f, float nms_threshold = 0.45f);

  std::vector<Detection> process(const std::vector<cv::Mat>& outputs,
                                 int image_width,
                                 int image_height,
                                 int input_width,
                                 int input_height);

private:
  float conf_threshold_;
  float nms_threshold_;

  bool isVehicle(int class_id) const;
  std::string getClassName(int class_id) const;
};

#endif // POSTPROCESSOR_HPP