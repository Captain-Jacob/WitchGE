#ifndef GESTURE_MODEL_HPP
#define GESTURE_MODEL_HPP

#include <string>
#include <vector>

#if defined(HAS_ONNX_RUNTIME) && HAS_ONNX_RUNTIME == 1
  #include <onnxruntime_cxx_api.h>
#elif __has_include(<onnxruntime_cxx_api.h>) || __has_include("onnxruntime_cxx_api.h")
  #include <onnxruntime_cxx_api.h>
  #ifndef HAS_ONNX_RUNTIME
    #define HAS_ONNX_RUNTIME 1
  #endif
#else
  #ifndef HAS_ONNX_RUNTIME
    #define HAS_ONNX_RUNTIME 0
  #endif
#endif

struct Detection {
    int classId;
    std::string label;
    float confidence;
    float x1, y1, x2, y2; // Bounding box in 640x640 space
};

std::vector<Detection> EvaluateGesture(const std::vector<float>& inputTensor, float confThreshold = 0.25f, float iouThreshold = 0.45f);
void SaveDebugImageWithBoxes(const std::vector<float>& tensor, const std::vector<Detection>& detections, const std::string& filename);

#endif // GESTURE_MODEL_HPP