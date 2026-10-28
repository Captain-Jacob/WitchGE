#include "GestureModel.hpp"
#include "Config.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>

static float CalculateIoU(const Detection& a, const Detection& b) {
    float interX1 = std::max(a.x1, b.x1);
    float interY1 = std::max(a.y1, b.y1);
    float interX2 = std::min(a.x2, b.x2);
    float interY2 = std::min(a.y2, b.y2);

    float interArea = std::max(0.0f, interX2 - interX1) * std::max(0.0f, interY2 - interY1);
    float areaA = (a.x2 - a.x1) * (a.y2 - a.y1);
    float areaB = (b.x2 - b.x1) * (b.y2 - b.y1);
    float unionArea = areaA + areaB - interArea;

    return unionArea > 0.0f ? interArea / unionArea : 0.0f;
}

static std::vector<Detection> ApplyNMS(std::vector<Detection>& candidates, float iouThreshold) {
    std::sort(candidates.begin(), candidates.end(), [](const Detection& a, const Detection& b) {
        return a.confidence > b.confidence;
    });

    std::vector<Detection> results;
    std::vector<bool> suppressed(candidates.size(), false);

    for (size_t i = 0; i < candidates.size(); ++i) {
        if (suppressed[i]) continue;
        results.push_back(candidates[i]);
        for (size_t j = i + 1; j < candidates.size(); ++j) {
            if (suppressed[j]) continue;
            if (candidates[i].classId == candidates[j].classId) {
                if (CalculateIoU(candidates[i], candidates[j]) > iouThreshold) {
                    suppressed[j] = true;
                }
            }
        }
    }
    return results;
}

std::vector<Detection> EvaluateGesture(const std::vector<float>& inputTensor, float confThreshold, float iouThreshold) {
    std::vector<Detection> candidates;

#if HAS_ONNX_RUNTIME == 1
    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "WitchEngine");
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

#ifdef _WIN32
        Ort::Session session(env, L"model/best.onnx", options);
#else
        Ort::Session session(env, "model/best.onnx", options);
#endif

        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<int64_t> inputShape = {1, 3, MODEL_INPUT_SIZE, MODEL_INPUT_SIZE};
        
        Ort::Value inputVal = Ort::Value::CreateTensor<float>(
            memoryInfo, const_cast<float*>(inputTensor.data()), inputTensor.size(), inputShape.data(), inputShape.size()
        );

        const char* inputNames[] = {"images"};
        const char* outputNames[] = {"output0"};

        auto outputs = session.Run(Ort::RunOptions{nullptr}, inputNames, &inputVal, 1, outputNames, 1);
        float* rawData = outputs[0].GetTensorMutableData<float>();

        int numAnchors = 8400;
        int numClasses = static_cast<int>(CLASS_LABELS.size());

        for (int anchor = 0; anchor < numAnchors; ++anchor) {
            float cx = rawData[0 * numAnchors + anchor];
            float cy = rawData[1 * numAnchors + anchor];
            float w  = rawData[2 * numAnchors + anchor];
            float h  = rawData[3 * numAnchors + anchor];

            int bestClass = -1;
            float highestScore = confThreshold;

            for (int cls = 0; cls < numClasses; ++cls) {
                float score = rawData[(4 + cls) * numAnchors + anchor];
                if (score > highestScore) {
                    highestScore = score;
                    bestClass = cls;
                }
            }

            if (bestClass != -1) {
                float x1 = std::clamp(cx - w * 0.5f, 0.0f, (float)MODEL_INPUT_SIZE);
                float y1 = std::clamp(cy - h * 0.5f, 0.0f, (float)MODEL_INPUT_SIZE);
                float x2 = std::clamp(cx + w * 0.5f, 0.0f, (float)MODEL_INPUT_SIZE);
                float y2 = std::clamp(cy + h * 0.5f, 0.0f, (float)MODEL_INPUT_SIZE);

                std::string label = (bestClass >= 0 && bestClass < numClasses) ? CLASS_LABELS[bestClass] : "unknown";
                candidates.push_back({bestClass, label, highestScore * 100.0f, x1, y1, x2, y2});
            }
        }

    } catch (const std::exception& e) {
        std::cerr << " [ONNX ERROR] " << e.what() << std::endl;
    }
#endif

    return ApplyNMS(candidates, iouThreshold);
}

void SaveDebugImageWithBoxes(const std::vector<float>& tensor, const std::vector<Detection>& detections, const std::string& filename) {
    int w = MODEL_INPUT_SIZE, h = MODEL_INPUT_SIZE;
    int filesize = 54 + 3 * w * h;

    // Convert normalized tensor to BGR buffer
    std::vector<unsigned char> imgData(w * h * 3);
    int planeSize = w * h;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            int pixelOffset = (y * w + x) * 3;

            imgData[pixelOffset + 2] = static_cast<unsigned char>(std::clamp(tensor[0 * planeSize + idx] * 255.0f, 0.0f, 255.0f)); // R
            imgData[pixelOffset + 1] = static_cast<unsigned char>(std::clamp(tensor[1 * planeSize + idx] * 255.0f, 0.0f, 255.0f)); // G
            imgData[pixelOffset + 0] = static_cast<unsigned char>(std::clamp(tensor[2 * planeSize + idx] * 255.0f, 0.0f, 255.0f)); // B
        }
    }

    // Draw detected bounding boxes (Red borders)
    auto drawBox = [&](int x1, int y1, int x2, int y2, int thickness = 3) {
        for (int t = 0; t < thickness; ++t) {
            for (int x = x1; x <= x2; ++x) {
                auto setRed = [&](int px, int py) {
                    if (px >= 0 && px < w && py >= 0 && py < h) {
                        int idx = (py * w + px) * 3;
                        imgData[idx + 0] = 0;   // B
                        imgData[idx + 1] = 0;   // G
                        imgData[idx + 2] = 255; // R
                    }
                };
                setRed(x, y1 + t);
                setRed(x, y2 - t);
            }
            for (int y = y1; y <= y2; ++y) {
                auto setRed = [&](int px, int py) {
                    if (px >= 0 && px < w && py >= 0 && py < h) {
                        int idx = (py * w + px) * 3;
                        imgData[idx + 0] = 0;   // B
                        imgData[idx + 1] = 0;   // G
                        imgData[idx + 2] = 255; // R
                    }
                };
                setRed(x1 + t, y);
                setRed(x2 - t, y);
            }
        }
    };

    for (const auto& det : detections) {
        drawBox(static_cast<int>(det.x1), static_cast<int>(det.y1), static_cast<int>(det.x2), static_cast<int>(det.y2));
    }

    // Write BMP File
    unsigned char bmpFileHeader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
    unsigned char bmpInfoHeader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};

    bmpFileHeader[2] = (unsigned char)(filesize);
    bmpFileHeader[3] = (unsigned char)(filesize >> 8);
    bmpFileHeader[4] = (unsigned char)(filesize >> 16);
    bmpFileHeader[5] = (unsigned char)(filesize >> 24);

    bmpInfoHeader[4] = (unsigned char)(w);
    bmpInfoHeader[5] = (unsigned char)(w >> 8);
    bmpInfoHeader[6] = (unsigned char)(w >> 16);
    bmpInfoHeader[7] = (unsigned char)(w >> 24);
    bmpInfoHeader[8] = (unsigned char)(h);
    bmpInfoHeader[9] = (unsigned char)(h >> 8);
    bmpInfoHeader[10] = (unsigned char)(h >> 16);
    bmpInfoHeader[11] = (unsigned char)(h >> 24);

    std::ofstream f(filename, std::ios::out | std::ios::binary);
    if (!f.is_open()) return;

    f.write((char*)bmpFileHeader, 14);
    f.write((char*)bmpInfoHeader, 40);

    for (int y = h - 1; y >= 0; --y) {
        f.write((char*)&imgData[y * w * 3], w * 3);
    }
    f.close();
    std::cout << " Saved debug image with bounding boxes to: '" << filename << "'" << std::endl;
}