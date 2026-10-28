#include <glad.h>
#include <GLFW/glfw3.h>
#include "InputHandler.hpp"
#include "GestureModel.hpp"
#include <iostream>

static InputState* g_inputState = nullptr;

void setInputStatePointer(InputState* state) {
    g_inputState = state;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (!g_inputState || !g_inputState->canvas) return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (g_inputState->canvas->isOpen) {
        if (g_inputState->canvas->isDrawing) {
            glm::vec2 localPos = glm::vec2(xpos, ypos) - g_inputState->canvas->currentTotalOffset;
            g_inputState->canvas->currentStroke.push_back(localPos);
        }
        return;
    }

    if (g_inputState->firstMouse) {
        g_inputState->lastX = xpos;
        g_inputState->lastY = ypos;
        g_inputState->firstMouse = false;
    }

    float xoffset = xpos - g_inputState->lastX;
    float yoffset = g_inputState->lastY - ypos;
    g_inputState->lastX = xpos;
    g_inputState->lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    g_inputState->yaw   += xoffset;
    g_inputState->pitch += yoffset;

    if (g_inputState->pitch > 89.0f)  g_inputState->pitch =  89.0f;
    if (g_inputState->pitch < -89.0f) g_inputState->pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(g_inputState->yaw)) * cos(glm::radians(g_inputState->pitch));
    front.y = sin(glm::radians(g_inputState->pitch));
    front.z = sin(glm::radians(g_inputState->yaw)) * cos(glm::radians(g_inputState->pitch));
    g_inputState->cameraFront = glm::normalize(front);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (!g_inputState || !g_inputState->canvas || !g_inputState->canvas->isOpen) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_inputState->canvas->isDrawing = true;
            g_inputState->canvas->currentStroke.clear();
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            glm::vec2 localPos = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos)) - g_inputState->canvas->currentTotalOffset;
            g_inputState->canvas->currentStroke.push_back(localPos);
        } else if (action == GLFW_RELEASE) {
            if (g_inputState->canvas->isDrawing) {
                g_inputState->canvas->isDrawing = false;
                if (!g_inputState->canvas->currentStroke.empty()) {
                    g_inputState->canvas->strokes.push_back(g_inputState->canvas->currentStroke);
                    g_inputState->canvas->currentStroke.clear();
                }
            }
        }
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (!g_inputState || !g_inputState->canvas) return;

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_J || key == GLFW_KEY_P) {
            if (g_inputState->canvas->isOpen) {
                std::cout << "\n=========================================" << std::endl;
                std::cout << " CASTING SPELL GESTURE (" << g_inputState->canvas->strokes.size() << " strokes)..." << std::endl;
                
                // 1. Direct rasterization without auto-cropping
                std::vector<float> inputTensor = RasterizeStrokesDirect(g_inputState->canvas->strokes, true);

                // 2. Multi-object detection with NMS
                std::vector<Detection> detections = EvaluateGesture(inputTensor, 0.20f, 0.45f);

                // 3. Save Snapshot with red bounding boxes overlayed
                SaveDebugImageWithBoxes(inputTensor, detections, "debug_output.bmp");

                std::cout << " DETECTED " << detections.size() << " SIGN(S):" << std::endl;
                for (size_t i = 0; i < detections.size(); ++i) {
                    const auto& det = detections[i];
                    std::cout << "  [" << i + 1 << "] Label: " << det.label 
                              << " | Conf: " << det.confidence << "%"
                              << " | Box: [" << det.x1 << ", " << det.y1 << ", " << det.x2 << ", " << det.y2 << "]" << std::endl;
                }
                std::cout << "=========================================\n" << std::endl;

                g_inputState->canvas->reset();
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                g_inputState->firstMouse = true;
            } else {
                g_inputState->canvas->isOpen = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
    }
}