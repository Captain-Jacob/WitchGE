#ifndef INPUT_HANDLER_HPP
#define INPUT_HANDLER_HPP

#include <glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "Canvas.hpp"

struct InputState {
    CanvasState* canvas = nullptr;
    glm::vec3 cameraFront = glm::vec3(0.0f, -0.2f, -1.0f);
    glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw   = -90.0f;
    float pitch =  0.0f;
    float lastX =  1280.0f / 2.0f;
    float lastY =  720.0f / 2.0f;
    bool firstMouse = true;
};

void setInputStatePointer(InputState* state);

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

#endif // INPUT_HANDLER_HPP