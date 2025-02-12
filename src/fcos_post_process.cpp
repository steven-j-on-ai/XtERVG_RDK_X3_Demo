// Copyright (c) 2020 Horizon Robotics.All Rights Reserved.
//
// The material in this file is confidential and contains trade secrets
// of Horizon Robotics Inc. This is proprietary information owned by
// Horizon Robotics Inc. No part of this work may be disclosed,
// reproduced, copied, transmitted, or used in any way for any purpose,
// without the express written permission of Horizon Robotics Inc.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>

#include "fcos_post_process.hpp"

using namespace std;

float score_hold = 0.45;
float iou_threshold = 0.6;
int top_k = 500;
/**
 * Config definition for Fcos
 */

void print_bin_data(uint8_t *data, int len)
{
	int iter = 0;
	if (!data || len <= 0) {
		return;
	}

	while (iter < len) {
		fprintf(stderr, "%02x ", data[iter++]);
		if (iter % (8 * 4) == 0) {
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\n");
}


PTQFcosConfig fcos_config_ = {
    {{8, 16, 32, 64, 128}},
    80,
    {"person", "bicycle", "car",
     "motorcycle", "airplane", "bus",
     "train", "truck", "boat",
     "traffic light", "fire hydrant", "stop sign",
     "parking meter", "bench", "bird",
     "cat", "dog", "horse",
     "sheep", "cow", "elephant",
     "bear", "zebra", "giraffe",
     "backpack", "umbrella", "handbag",
     "tie", "suitcase", "frisbee",
     "skis", "snowboard", "sports ball",
     "kite", "baseball bat", "baseball glove",
     "skateboard", "surfboard", "tennis racket",
     "bottle", "wine glass", "cup",
     "fork", "knife", "spoon",
     "bowl", "banana", "apple",
     "sandwich", "orange", "broccoli",
     "carrot", "hot dog", "pizza",
     "donut", "cake", "chair",
     "couch", "potted plant", "bed",
     "dining table", "toilet", "tv",
     "laptop", "mouse", "remote",
     "keyboard", "cell phone", "microwave",
     "oven", "toaster", "sink",
     "refrigerator", "book", "clock",
     "vase", "scissors", "teddy bear",
     "hair drier", "toothbrush"},
    ""};

static int get_tensor_hwc_index(hbDNNTensor *tensor,
                                int *h_index,
                                int *w_index,
                                int *c_index)
{
  if (tensor->properties.tensorLayout == HB_DNN_LAYOUT_NHWC)
  {
    *h_index = 1;
    *w_index = 2;
    *c_index = 3;
  }
  else if (tensor->properties.tensorLayout == HB_DNN_LAYOUT_NCHW)
  {
    *c_index = 1;
    *h_index = 2;
    *w_index = 3;
  }
  else
  {
    return -1;
  }
  return 0;
}

static void fcos_nms(std::vector<Detection> &input,
                     float iou_threshold,
                     int top_k,
                     std::vector<Detection> &result,
                     bool suppress)
{
  // sort order by score desc
  std::stable_sort(input.begin(), input.end(), std::greater<Detection>());

  std::vector<bool> skip(input.size(), false);

  // pre-calculate boxes area
  std::vector<float> areas;
  areas.reserve(input.size());
  for (size_t i = 0; i < input.size(); i++)
  {
    float width = input[i].bbox.xmax - input[i].bbox.xmin;
    float height = input[i].bbox.ymax - input[i].bbox.ymin;
    areas.push_back(width * height);
  }

  int count = 0;
  for (size_t i = 0; count < top_k && i < skip.size(); i++)
  {
    if (skip[i])
    {
      continue;
    }
    skip[i] = true;
    ++count;

    for (size_t j = i + 1; j < skip.size(); ++j)
    {
      if (skip[j])
      {
        continue;
      }
      if (suppress == false)
      {
        if (input[i].id != input[j].id)
        {
          continue;
        }
      }

      // intersection area
      float xx1 = std::max(input[i].bbox.xmin, input[j].bbox.xmin);
      float yy1 = std::max(input[i].bbox.ymin, input[j].bbox.ymin);
      float xx2 = std::min(input[i].bbox.xmax, input[j].bbox.xmax);
      float yy2 = std::min(input[i].bbox.ymax, input[j].bbox.ymax);

      if (xx2 > xx1 && yy2 > yy1)
      {
        float area_intersection = (xx2 - xx1) * (yy2 - yy1);
        float iou_ratio =
            area_intersection / (areas[j] + areas[i] - area_intersection);
        if (iou_ratio > iou_threshold)
        {
          skip[j] = true;
        }
      }
    }
    result.push_back(input[i]);
  }
}

static void GetBboxAndScoresNHWC(
    hbDNNTensor *tensors,
    bpu_image_info_t *post_info,
    std::vector<Detection> &dets)
{
  int ori_h = post_info->m_ori_height;
  int ori_w = post_info->m_ori_width;
  int input_h = post_info->m_model_h;
  int input_w = post_info->m_model_w;
  float w_scale;
  float h_scale;
  // preprocess action is pad and resize
  w_scale = static_cast<float>(ori_w) / input_w;
  h_scale = static_cast<float>(ori_h) / input_h;

  // fcos stride is {8, 16, 32, 64, 128}
  for (int i = 0; i < 5; i++)
  {
    auto *cls_data = reinterpret_cast<float *>(tensors[i].sysMem[0].virAddr);
    auto *bbox_data =
        reinterpret_cast<float *>(tensors[i + 5].sysMem[0].virAddr);
    auto *ce_data =
        reinterpret_cast<float *>(tensors[i + 10].sysMem[0].virAddr);

    // 同一个尺度下，tensor[i],tensor[i+5],tensor[i+10]出来的hw都一致，64*64/32*32/...
    int *shape = tensors[i].properties.alignedShape.dimensionSize;
    int tensor_h = shape[1];
    int tensor_w = shape[2];
    int tensor_c = shape[3];

    for (int h = 0; h < tensor_h; h++)
    {
      int offset = h * tensor_w;
      for (int w = 0; w < tensor_w; w++)
      {
        // get score
        int ce_offset = offset + w;
        ce_data[ce_offset] = 1.0 / (1.0 + exp(-ce_data[ce_offset]));

        int cls_offset = ce_offset * tensor_c;
        ScoreId tmp_score = {cls_data[cls_offset], 0};
        for (int cls_c = 1; cls_c < tensor_c; cls_c++)
        {
          int cls_index = cls_offset + cls_c;
          if (cls_data[cls_index] > tmp_score.score)
          {
            tmp_score.id = cls_c;
            tmp_score.score = cls_data[cls_index];
          }
        }
        tmp_score.score = 1.0 / (1.0 + exp(-tmp_score.score));
        tmp_score.score = std::sqrt(tmp_score.score * ce_data[ce_offset]);
        if (tmp_score.score <= score_hold)
          continue;

        // get detection box
        Detection detection;
        int index = 4 * (h * tensor_w + w);
        auto &strides = fcos_config_.strides;

        detection.bbox.xmin =
            ((w + 0.5) * strides[i] - bbox_data[index]) * w_scale;
        detection.bbox.ymin =
            ((h + 0.5) * strides[i] - bbox_data[index + 1]) * h_scale;
        detection.bbox.xmax =
            ((w + 0.5) * strides[i] + bbox_data[index + 2]) * w_scale;
        detection.bbox.ymax =
            ((h + 0.5) * strides[i] + bbox_data[index + 3]) * h_scale;

        detection.score = tmp_score.score;
        detection.id = tmp_score.id;
        detection.class_name = fcos_config_.class_names[detection.id].c_str();
        dets.push_back(detection);
      }
    }
  }
}

static void GetBboxAndScoresNCHW(
    hbDNNTensor *tensors,
    bpu_image_info_t *post_info,
    std::vector<Detection> &dets)
{
  int ori_h = post_info->m_ori_height;
  int ori_w = post_info->m_ori_width;
  int input_h = post_info->m_model_h;
  int input_w = post_info->m_model_w;
  float w_scale;
  float h_scale;
  // preprocess action is pad and resize
  w_scale = static_cast<float>(ori_w) / input_w;
  h_scale = static_cast<float>(ori_h) / input_h;

  for (int i = 0; i < 5; i++)
  {
    auto *cls_data = reinterpret_cast<float *>(tensors[i].sysMem[0].virAddr);
    auto *bbox_data =
        reinterpret_cast<float *>(tensors[i + 5].sysMem[0].virAddr);
    auto *ce_data =
        reinterpret_cast<float *>(tensors[i + 10].sysMem[0].virAddr);

    // 同一个尺度下，tensor[i],tensor[i+5],tensor[i+10]出来的hw都一致，64*64/32*32/...
    int *shape = tensors[i].properties.alignedShape.dimensionSize;
    int tensor_c = shape[1];
    int tensor_h = shape[2];
    int tensor_w = shape[3];
    int aligned_hw = tensor_h * tensor_w;

    for (int h = 0; h < tensor_h; h++)
    {
      int offset = h * tensor_w;
      for (int w = 0; w < tensor_w; w++)
      {
        // get score
        int ce_offset = offset + w;
        ce_data[ce_offset] = 1.0 / (1.0 + exp(-ce_data[ce_offset]));

        ScoreId tmp_score = {cls_data[offset + w], 0};
        for (int cls_c = 1; cls_c < tensor_c; cls_c++)
        {
          int cls_index = cls_c * aligned_hw + offset + w;
          if (cls_data[cls_index] > tmp_score.score)
          {
            tmp_score.id = cls_c;
            tmp_score.score = cls_data[cls_index];
          }
        }
        tmp_score.score = 1.0 / (1.0 + exp(-tmp_score.score));
        tmp_score.score = std::sqrt(tmp_score.score * ce_data[ce_offset]);
        if (tmp_score.score <= score_hold)
          continue;

        // get detection box
        auto &strides = fcos_config_.strides;
        Detection detection;
        detection.bbox.xmin =
            ((w + 0.5) * strides[i] - bbox_data[offset + w]) * w_scale;
        detection.bbox.ymin =
            ((h + 0.5) * strides[i] - bbox_data[1 * aligned_hw + offset + w]) *
            h_scale;
        detection.bbox.xmax =
            ((w + 0.5) * strides[i] + bbox_data[2 * aligned_hw + offset + w]) *
            w_scale;
        detection.bbox.ymax =
            ((h + 0.5) * strides[i] + bbox_data[3 * aligned_hw + offset + w]) *
            h_scale;

        detection.score = tmp_score.score;
        detection.id = tmp_score.id;
        detection.class_name = fcos_config_.class_names[detection.id].c_str();
        dets.push_back(detection);
      }
    }
  }
}

void fcos_post_process(hbDNNTensor* tensors, bpu_image_info_t *post_info, std::vector<Detection> &det_restuls)
{

  std::vector<Detection> dets;

  int h_index, w_index, c_index;
  int ret = get_tensor_hwc_index(&tensors[0], &h_index, &w_index, &c_index);
  if (ret)
  {
    printf(" %s [ERROR]:Invalid tensor,please check your model!\n", __FUNCTION__);
    return ;
  }
  if (tensors[0].properties.tensorLayout == HB_DNN_LAYOUT_NHWC)
  {
    GetBboxAndScoresNHWC(tensors, post_info, dets);
  }
  else if (tensors[0].properties.tensorLayout == HB_DNN_LAYOUT_NCHW)
  {
    GetBboxAndScoresNCHW(tensors, post_info, dets);
  }
  else
  {
    printf("tensor layout error.\n");
    return ;
  }
  // 计算交并比来合并检测框，传入交并比阈值和返回box数量
  fcos_nms(dets, iou_threshold, top_k, det_restuls, false);
}
