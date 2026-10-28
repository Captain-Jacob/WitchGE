#ifndef CANVAS_HPP
#define CANVAS_HPP

#include <vector>
#include <glm/glm.hpp>

struct CanvasState {
    bool isOpen = false;
    float slideAnim = 0.0f;
    bool isDrawing = false;
    std::vector<std::vector<glm::vec2>> strokes;
    std::vector<glm::vec2> currentStroke;
    glm::vec2 shakeOffset = glm::vec2(0.0f);
    glm::vec2 currentTotalOffset = glm::vec2(0.0f);

    void reset();
};

// Direct rasterization without auto-cropping or auto-rescaling
std::vector<float> RasterizeStrokesDirect(const std::vector<std::vector<glm::vec2>>& strokes, bool invertColors = true);

#endif // CANVAS_HPP