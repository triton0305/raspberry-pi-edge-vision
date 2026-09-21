#include "postprocessor.hpp"

#include <opencv2/dnn.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

PostProcessor::PostProcessor(float conf_threshold, float nms_threshold)
  : conf_threshold_(conf_threshold), nms_threshold_(nms_threshold)
{
}

std::vector<Detection> PostProcessor::process(
  const std::vector<cv::Mat>& outputs,
  int image_width,
  int image_height,
  int input_width,
  int input_height)
{
  std::vector<Detection> results;

  if (outputs.empty())
  {
    std::cerr << "PostProcessor: empty output\n";
    return results;
  }

  const cv::Mat& output = outputs[0];

  if (output.dims != 3 || output.size[1] != 84)
  {
    std::cerr << "PostProcessor: unexpected output shape\n";
    return results;
  }

  cv::Mat raw = output.reshape(1, 84);
  cv::Mat detections;
  cv::transpose(raw, detections);

  const float x_factor = static_cast<float>(image_width) / input_width;
  const float y_factor = static_cast<float>(image_height) / input_height;

  std::vector<cv::Rect> boxes;
  std::vector<float> confidences;
  std::vector<int> class_ids;

  for (int i = 0; i < detections.rows; ++i)
  {
    float* data = detections.ptr<float>(i);

    const float cx = data[0];
    const float cy = data[1];
    const float width = data[2];
    const float height = data[3];

    cv::Mat scores(1, 80, CV_32F, data + 4);

    cv::Point class_id_point;
    double max_score;

    cv::minMaxLoc(scores, nullptr, &max_score, nullptr, &class_id_point);

    const int class_id = class_id_point.x;
    const float confidence = static_cast<float>(max_score);

    if (!isVehicle(class_id) || confidence < conf_threshold_)
      continue;

    int left = static_cast<int>((cx - width / 2.0f) * x_factor);
    int top = static_cast<int>((cy - height / 2.0f) * y_factor);
    int box_width = static_cast<int>(width * x_factor);
    int box_height = static_cast<int>(height * y_factor);

    left = std::max(0, left);
    top = std::max(0, top);

    box_width = std::min(box_width, image_width - left);
    box_height = std::min(box_height, image_height - top);

    if (box_width <= 0 || box_height <= 0)
      continue;

    boxes.emplace_back(left, top, box_width, box_height);
    confidences.push_back(confidence);
    class_ids.push_back(class_id);
  }

  std::vector<int> indices;

  cv::dnn::NMSBoxes(
    boxes, confidences, conf_threshold_, nms_threshold_, indices);

  for (int index : indices)
  {
    const cv::Rect& box = boxes[index];

    Detection detection;

    detection.class_id = class_ids[index];
    detection.class_name = getClassName(class_ids[index]);
    detection.confidence = confidences[index];
    detection.bbox = {box.x, box.y, box.width, box.height};

    results.push_back(detection);
  }

  return results;
}

bool PostProcessor::isVehicle(int class_id) const
{
  return class_id == 2 ||
         class_id == 3 ||
         class_id == 5 ||
         class_id == 7;
}

std::string PostProcessor::getClassName(int class_id) const
{
  switch (class_id)
  {
    case 2:
      return "car";
    case 3:
      return "motorcycle";
    case 5:
      return "bus";
    case 7:
      return "truck";
    default:
      return "unknown";
  }
}