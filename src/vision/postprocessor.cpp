#include "vision/postprocessor.hpp"

#include <opencv2/dnn.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>

PostProcessor::PostProcessor(float conf_threshold, float nms_threshold)
  : conf_threshold_(conf_threshold), nms_threshold_(nms_threshold)
{
}

std::vector<Detection> PostProcessor::process(
  const std::vector<cv::Mat>& outputs,
  int image_width, int image_height,
  int input_width, int input_height)
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

  const float scale = std::min(
    static_cast<float>(input_width) / image_width,
    static_cast<float>(input_height) / image_height);

  const int resized_width = static_cast<int>(std::round(image_width * scale));
  const int resized_height = static_cast<int>(std::round(image_height * scale));

  const int pad_x = (input_width - resized_width) / 2;
  const int pad_y = (input_height - resized_height) / 2;

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

    int left = static_cast<int>((cx - width / 2.0f - pad_x) / scale);
    int top = static_cast<int>((cy - height / 2.0f - pad_y) / scale);
    int right = static_cast<int>((cx + width / 2.0f - pad_x) / scale);
    int bottom = static_cast<int>((cy + height / 2.0f - pad_y) / scale);

    left = std::clamp(left, 0, image_width);
    top = std::clamp(top, 0, image_height);
    right = std::clamp(right, 0, image_width);
    bottom = std::clamp(bottom, 0, image_height);

    int box_width = right - left;
    int box_height = bottom - top;

    if (box_width <= 0 || box_height <= 0)
      continue;

    boxes.emplace_back(left, top, box_width, box_height);
    confidences.push_back(confidence);
    class_ids.push_back(class_id);
  }

  std::vector<int> indices;
  const int vehicle_classes[] = {2, 3, 5, 7};

  for (int vehicle_class : vehicle_classes)
  {
    std::vector<cv::Rect> class_boxes;
    std::vector<float> class_confidences;
    std::vector<int> class_indices;

    for (std::size_t i = 0; i < class_ids.size(); ++i)
    {
      if (class_ids[i] != vehicle_class)
        continue;

      class_boxes.push_back(boxes[i]);
      class_confidences.push_back(confidences[i]);
      class_indices.push_back(static_cast<int>(i));
    }

    std::vector<int> nms_indices;
    cv::dnn::NMSBoxes(class_boxes, class_confidences, conf_threshold_, nms_threshold_, nms_indices);

    for (int index : nms_indices)
      indices.push_back(class_indices[index]);
  }

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
  return class_id == 2 || class_id == 3 || class_id == 5 || class_id == 7;
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