/*
 * @Author: jiale01.luo
 * @Date: 2022-10-28 21:01:15
 * @Last Modified by: jiale01.luo
 * @Last Modified time: 2022-10-28 21:03:24
 */
#ifndef fcos_post
#define fcos_post

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <dnn/hb_dnn.h>
#include <cmath>
#include <algorithm>
#include "sp_bpu.h"
#include <iostream>
#include <iomanip>

using namespace std;

struct PTQFcosConfig {
  std::vector<int> strides;
  int class_num;
  std::vector<std::string> class_names;
  std::string det_name_list;

  std::string Str() {
    std::stringstream ss;
    ss << "strides: ";
    for (const auto &stride : strides) {
      ss << stride << " ";
    }
    ss << "; class_num: " << class_num;
    return ss.str();
  }
};

struct ScoreId {
  float score;
  int id;
};

/**
 * Finds the smallest element in the range [first, last).
 * @tparam[in] ForwardIterator
 * @para[in] first: first iterator
 * @param[in] last: last iterator
 * @return Iterator to the smallest element in the range [first, last)
 */
template <class ForwardIterator>
inline size_t argmin(ForwardIterator first, ForwardIterator last) {
  return std::distance(first, std::min_element(first, last));
}
 
// /**
//  * Finds the greatest element in the range [first, last)
//  * @tparam[in] ForwardIterator: iterator type
//  * @param[in] first: fist iterator
//  * @param[in] last: last iterator
//  * @return Iterator to the greatest element in the range [first, last)
//  */
// template <class ForwardIterator>
// inline size_t argmax(ForwardIterator first, ForwardIterator last) {
//   return std::distance(first, std::max_element(first, last));
// }

/**
 * Bounding box definition
 */
typedef struct Bbox {
  float xmin;
  float ymin;
  float xmax;
  float ymax;
             
  Bbox() {}  
             
  Bbox(float xmin, float ymin, float xmax, float ymax)
      : xmin(xmin), ymin(ymin), xmax(xmax), ymax(ymax) {}
             
  friend std::ostream &operator<<(std::ostream &os, const Bbox &bbox) {
    os << "[" << std::fixed << std::setprecision(6) << bbox.xmin << ","
       << bbox.ymin << "," << bbox.xmax << "," << bbox.ymax << "]";
    return os; 
  }          
             
  ~Bbox() {} 
} Bbox;

typedef struct Detection {
  int id; 
  float score;
  Bbox bbox;
  const char *class_name;
  Detection() {}
 
  Detection(int id, float score, Bbox bbox)
      : id(id), score(score), bbox(bbox) {}
 
  Detection(int id, float score, Bbox bbox, const char *class_name)
      : id(id), score(score), bbox(bbox), class_name(class_name) {}
 
  friend bool operator>(const Detection &lhs, const Detection &rhs) {
    return (lhs.score > rhs.score);
  }
 
 
  ~Detection() {}
} Detection;

//extern FcosConfig default_fcos_config;
void fcos_post_process(hbDNNTensor* tensors ,bpu_image_info_t *post_info,std::vector<Detection> &det_restuls);

#endif
