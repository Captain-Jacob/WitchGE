#include <glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cstdlib>

#include "Config.hpp"
#include "Player.hpp"
#include "Canvas.hpp"
#include "GestureModel.hpp"
#include "Shader.hpp"
#include "InputHandler.hpp"

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Dream of a Witch - WitchEngine", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW Window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    Player player;
    CanvasState canvas;
    InputState inputState;
    inputState.canvas = &canvas;

    setInputStatePointer(&inputState);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    GLuint shader3D = createShaderProgram("shaders/vertex.glsl", "shaders/fragment.glsl");
    GLuint shaderUI = createShaderProgram("shaders/ui_vertex.glsl", "shaders/ui_fragment.glsl");

    std::vector<float> gridVertices;
    float gridSize = 60.0f;
    float gridStep = 2.0f;

    for (float i = -gridSize; i <= gridSize; i += gridStep) {
        gridVertices.insert(gridVertices.end(), { -gridSize, 0.0f, i,  0.3f, 0.35f, 0.4f });
        gridVertices.insert(gridVertices.end(), {  gridSize, 0.0f, i,  0.3f, 0.35f, 0.4f });
        gridVertices.insert(gridVertices.end(), { i, 0.0f, -gridSize,  0.3f, 0.35f, 0.4f });
        gridVertices.insert(gridVertices.end(), { i, 0.0f,  gridSize,  0.3f, 0.35f, 0.4f });
    }

    GLuint VAO3D, VBO3D;
    glGenVertexArrays(1, &VAO3D);
    glGenBuffers(1, &VBO3D);

    glBindVertexArray(VAO3D);
    glBindBuffer(GL_ARRAY_BUFFER, VBO3D);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    float panelHeight = 450.0f;
    std::vector<float> panelVertices = {
        0.0f,            SCR_HEIGHT - panelHeight, 0.95f, 0.95f, 0.95f, 0.92f,
        (float)SCR_WIDTH, SCR_HEIGHT - panelHeight, 0.95f, 0.95f, 0.95f, 0.92f,
        0.0f,            (float)SCR_HEIGHT,       0.95f, 0.95f, 0.95f, 0.92f,

        0.0f,            (float)SCR_HEIGHT,       0.95f, 0.95f, 0.95f, 0.92f,
        (float)SCR_WIDTH, SCR_HEIGHT - panelHeight, 0.95f, 0.95f, 0.95f, 0.92f,
        (float)SCR_WIDTH, (float)SCR_HEIGHT,       0.95f, 0.95f, 0.95f, 0.92f,
    };

    GLuint VAOUI, VBOUI;
    glGenVertexArrays(1, &VAOUI);
    glGenBuffers(1, &VBOUI);

    glBindVertexArray(VAOUI);
    glBindBuffer(GL_ARRAY_BUFFER, VBOUI);
    glBufferData(GL_ARRAY_BUFFER, panelVertices.size() * sizeof(float), panelVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint VAOLines, VBOLines;
    glGenVertexArrays(1, &VAOLines);
    glGenBuffers(1, &VBOLines);

    glBindVertexArray(VAOLines);
    glBindBuffer(GL_ARRAY_BUFFER, VBOLines);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    std::cout << "==================================================" << std::endl;
    std::cout << " WitchGE: Native Drawing Recognition Active" << std::endl;
    std::cout << "==================================================" << std::endl;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        player.update(window, deltaTime, inputState.cameraFront, inputState.cameraUp);

        float targetSlide = canvas.isOpen ? 1.0f : 0.0f;
        canvas.slideAnim = glm::mix(canvas.slideAnim, targetSlide, 12.0f * deltaTime);

        float horizSpeed = glm::length(glm::vec2(player.velocity.x, player.velocity.z));
        canvas.shakeOffset = glm::vec2(0.0f);
        
        if (canvas.slideAnim > 0.01f && horizSpeed > 0.8f) {
            float intensity = std::min((horizSpeed - 0.8f) * 3.5f, 35.0f);
            canvas.shakeOffset.x = ((rand() % 2000 - 1000) / 1000.0f) * intensity;
            canvas.shakeOffset.y = ((rand() % 2000 - 1000) / 1000.0f) * intensity;
        }

        float slideY = (1.0f - canvas.slideAnim) * panelHeight;
        canvas.currentTotalOffset = glm::vec2(canvas.shakeOffset.x, slideY + canvas.shakeOffset.y);

        int fps = static_cast<int>(1.0f / (deltaTime > 0.0001f ? deltaTime : 0.016f));
        std::string titleStr = "Dream of a Witch | Speed: " + std::to_string(horizSpeed).substr(0, 4) + 
                               " m/s | Slate: " + (canvas.isOpen ? "OPEN" : "CLOSED") + 
                               " | FPS: " + std::to_string(fps);
        glfwSetWindowTitle(window, titleStr.c_str());

        // --- Render 3D Scene ---
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader3D);

        glm::vec3 cameraPos = player.position + glm::vec3(0.0f, player.cameraHeight, 0.0f);
        glm::mat4 projection3D = glm::perspective(glm::radians(65.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 150.0f);
        glm::mat4 view3D = glm::lookAt(cameraPos, cameraPos + inputState.cameraFront, inputState.cameraUp);
        glm::mat4 model3D = glm::mat4(1.0f);

        glUniformMatrix4fv(glGetUniformLocation(shader3D, "model"), 1, GL_FALSE, glm::value_ptr(model3D));
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "view"), 1, GL_FALSE, glm::value_ptr(view3D));
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "projection"), 1, GL_FALSE, glm::value_ptr(projection3D));

        glBindVertexArray(VAO3D);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(gridVertices.size() / 6));

        // --- Render 2D Canvas & UI ---
        if (canvas.slideAnim > 0.001f) {
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUseProgram(shaderUI);

            glm::mat4 orthoProj = glm::ortho(0.0f, (float)SCR_WIDTH, (float)SCR_HEIGHT, 0.0f, -1.0f, 1.0f);
            glUniformMatrix4fv(glGetUniformLocation(shaderUI, "projection"), 1, GL_FALSE, glm::value_ptr(orthoProj));
            glUniform2fv(glGetUniformLocation(shaderUI, "offset"), 1, glm::value_ptr(canvas.currentTotalOffset));

            glBindVertexArray(VAOUI);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            std::vector<float> lineData;
            auto addStrokeToBuffer = [&](const std::vector<glm::vec2>& stroke) {
                if (stroke.size() < 2) return;
                for (size_t i = 0; i < stroke.size() - 1; ++i) {
                    lineData.push_back(stroke[i].x);
                    lineData.push_back(stroke[i].y);
                    lineData.push_back(0.1f); lineData.push_back(0.1f); lineData.push_back(0.1f); lineData.push_back(1.0f);

                    lineData.push_back(stroke[i+1].x);
                    lineData.push_back(stroke[i+1].y);
                    lineData.push_back(0.1f); lineData.push_back(0.1f); lineData.push_back(0.1f); lineData.push_back(1.0f);
                }
            };

            for (const auto& stroke : canvas.strokes) {
                addStrokeToBuffer(stroke);
            }
            addStrokeToBuffer(canvas.currentStroke);

            if (!lineData.empty()) {
                glLineWidth(4.0f);
                glBindVertexArray(VAOLines);
                glBindBuffer(GL_ARRAY_BUFFER, VBOLines);
                glBufferData(GL_ARRAY_BUFFER, lineData.size() * sizeof(float), lineData.data(), GL_DYNAMIC_DRAW);
                glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineData.size() / 6));
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO3D);
    glDeleteBuffers(1, &VBO3D);
    glDeleteVertexArrays(1, &VAOUI);
    glDeleteBuffers(1, &VBOUI);
    glDeleteVertexArrays(1, &VBOLines);
    glDeleteProgram(shader3D);
    glDeleteProgram(shaderUI);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}