#include "Canvas.hpp"
#include "Config.hpp"
#include <algorithm>
#include <cmath>

void CanvasState::reset() {
    strokes.clear();
    currentStroke.clear();
    isOpen = false;
}

std::vector<float> RasterizeStrokesDirect(const std::vector<std::vector<glm::vec2>>& strokes, bool invertColors) {
    int planeSize = MODEL_INPUT_SIZE * MODEL_INPUT_SIZE;
    float bgVal = invertColors ? 1.0f : 0.0f;
    float strokeVal = invertColors ? 0.0f : 1.0f;

    std::vector<float> tensor(1 * 3 * planeSize, bgVal);
    if (strokes.empty()) return tensor;

    // Direct mapping from Screen Space (1280x720) to Model Input Space (640x640)
    float scaleX = static_cast<float>(MODEL_INPUT_SIZE) / static_cast<float>(SCR_WIDTH);
    float scaleY = static_cast<float>(MODEL_INPUT_SIZE) / static_cast<float>(SCR_HEIGHT);

    auto drawThickPixel = [&](int cx, int cy, int radius = 6) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                int px = cx + dx;
                int py = cy + dy;
                if (px >= 0 && px < MODEL_INPUT_SIZE && py >= 0 && py < MODEL_INPUT_SIZE) {
                    if (dx * dx + dy * dy <= radius * radius) {
                        int pixelOffset = py * MODEL_INPUT_SIZE + px;
                        tensor[0 * planeSize + pixelOffset] = strokeVal;
                        tensor[1 * planeSize + pixelOffset] = strokeVal;
                        tensor[2 * planeSize + pixelOffset] = strokeVal;
                    }
                }
            }
        }
    };

    auto drawLine = [&](int x0, int y0, int x1, int y1) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, e2;

        while (true) {
            drawThickPixel(x0, y0, 6);
            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    };

    for (const auto& stroke : strokes) {
        if (stroke.size() < 2) continue;
        for (size_t i = 0; i < stroke.size() - 1; ++i) {
            int x0 = static_cast<int>(stroke[i].x * scaleX);
            int y0 = static_cast<int>(stroke[i].y * scaleY);
            int x1 = static_cast<int>(stroke[i+1].x * scaleX);
            int y1 = static_cast<int>(stroke[i+1].y * scaleY);
            drawLine(x0, y0, x1, y1);
        }
    }

    return tensor;
}