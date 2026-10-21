#include <glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// --- Player & Movement State Machine ---
struct Player {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    
    float cameraHeight = 1.6f;       // Smoothly lerps during crouch
    bool isGrounded = true;
    bool isCrouching = false;
    
    // Physics Parameters
    const float MAX_GROUND_SPEED = 8.0f;
    const float MAX_CROUCH_SPEED = 4.0f;
    const float GROUND_ACCEL     = 14.0f;
    const float GROUND_FRICTION  = 6.0f;
    const float AIR_ACCEL        = 45.0f;  // Quake/Source strafe accel
    const float JUMP_IMPULSE     = 6.5f;
    const float GRAVITY          = 20.0f;
} player;

// --- Camera Look State ---
glm::vec3 cameraFront = glm::vec3(0.0f, -0.2f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);

float yaw   = -90.0f;
float pitch =  0.0f;
float lastX =  SCR_WIDTH / 2.0f;
float lastY =  SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)  pitch =  89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// --- Movement Physics Helper Functions ---
void ApplyFriction(float dt) {
    float speed = glm::length(glm::vec2(player.velocity.x, player.velocity.z));
    if (speed < 0.0001f) {
        player.velocity.x = 0.0f;
        player.velocity.z = 0.0f;
        return;
    }
    float drop = speed * player.GROUND_FRICTION * dt;
    float newSpeed = std::max(0.0f, speed - drop);
    newSpeed /= speed;
    player.velocity.x *= newSpeed;
    player.velocity.z *= newSpeed;
}

void Accelerate(glm::vec3 wishdir, float wishspeed, float accel, float dt) {
    float currentspeed = glm::dot(player.velocity, wishdir);
    float addspeed = wishspeed - currentspeed;
    if (addspeed <= 0) return;
    float accelspeed = accel * dt * wishspeed;
    if (accelspeed > addspeed) accelspeed = addspeed;
    player.velocity += wishdir * accelspeed;
}

void UpdatePlayerPhysics(GLFWwindow* window, float dt) {
    // 1. Crouch State (C Key)
    player.isCrouching = (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS);
    float targetCameraHeight = player.isCrouching ? 0.8f : 1.6f;
    player.cameraHeight = glm::mix(player.cameraHeight, targetCameraHeight, 12.0f * dt);

    // 2. Input Wish Direction Matrix
    glm::vec3 forward = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
    glm::vec3 right = glm::normalize(glm::cross(forward, cameraUp));
    
    glm::vec3 wishdir(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishdir += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishdir -= forward;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishdir += right;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishdir -= right;

    if (glm::length(wishdir) > 0.001f) {
        wishdir = glm::normalize(wishdir);
    }

    float targetMaxSpeed = player.isCrouching ? player.MAX_CROUCH_SPEED : player.MAX_GROUND_SPEED;

    // 3. Ground vs Air Physics Pipeline
    if (player.isGrounded) {
        ApplyFriction(dt);
        if (glm::length(wishdir) > 0.001f) {
            Accelerate(wishdir, targetMaxSpeed, player.GROUND_ACCEL, dt);
        }
        
        // Jump Impulse
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            player.velocity.y = player.JUMP_IMPULSE;
            player.isGrounded = false;
        }
    } else {
        // Air Movement (Air Acceleration allows Bunny Hopping speed gain)
        if (glm::length(wishdir) > 0.001f) {
            Accelerate(wishdir, targetMaxSpeed, player.AIR_ACCEL, dt);
        }
        // Apply Gravity
        player.velocity.y -= player.GRAVITY * dt;
    }

    // 4. Integrate Position & Ground Collision
    player.position += player.velocity * dt;

    if (player.position.y <= 0.0f) {
        player.position.y = 0.0f;
        player.velocity.y = 0.0f;
        player.isGrounded = true;
    }
}

std::string loadShaderSource(const char* filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << filepath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint compileShader(GLenum type, const char* filepath) {
    std::string source = loadShaderSource(filepath);
    const char* srcPtr = source.c_str();
    
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &srcPtr, NULL);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Shader Compilation Error (" << filepath << "):\n" << infoLog << std::endl;
    }
    return shader;
}

GLuint createShaderProgram(const char* vertPath, const char* fragPath) {
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertPath);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragPath);

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "Program Link Error:\n" << infoLog << std::endl;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return program;
}

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
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    GLuint shaderProgram = createShaderProgram("shaders/vertex.glsl", "shaders/fragment.glsl");

    // 3D Grid Floor
    std::vector<float> gridVertices;
    float gridSize = 60.0f;
    float gridStep = 2.0f;

    for (float i = -gridSize; i <= gridSize; i += gridStep) {
        gridVertices.insert(gridVertices.end(), { -gridSize, 0.0f, i,  0.3f, 0.35f, 0.4f });
        gridVertices.insert(gridVertices.end(), {  gridSize, 0.0f, i,  0.3f, 0.35f, 0.4f });
        gridVertices.insert(gridVertices.end(), { i, 0.0f, -gridSize,  0.3f, 0.35f, 0.4f });
        gridVertices.insert(gridVertices.end(), { i, 0.0f,  gridSize,  0.3f, 0.35f, 0.4f });
    }

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    std::cout << "==================================================" << std::endl;
    std::cout << "🚀 Phase 2 Initialized: 3D Bunny Hop & Crouch Engine" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - WASD : Move / Strafe" << std::endl;
    std::cout << "  - SPACE : Jump (Time jumps while turning to Bunny Hop!)" << std::endl;
    std::cout << "  - C     : Crouch" << std::endl;
    std::cout << "  - ESC   : Exit" << std::endl;
    std::cout << "==================================================" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Update Physics
        UpdatePlayerPhysics(window, deltaTime);

        // Update Title Bar with Horizontal Speed & FPS Metrics
        float horizSpeed = glm::length(glm::vec2(player.velocity.x, player.velocity.z));
        int fps = static_cast<int>(1.0f / (deltaTime > 0.0001f ? deltaTime : 0.016f));
        std::string titleStr = "Dream of a Witch | Speed: " + std::to_string(horizSpeed).substr(0, 4) + 
                               " m/s | FPS: " + std::to_string(fps) + 
                               (player.isCrouching ? " [CROUCHING]" : "");
        glfwSetWindowTitle(window, titleStr.c_str());

        // Rendering
        glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Camera Position based on Player state
        glm::vec3 cameraPos = player.position + glm::vec3(0.0f, player.cameraHeight, 0.0f);

        glm::mat4 projection = glm::perspective(glm::radians(65.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 150.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 model = glm::mat4(1.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(gridVertices.size() / 6));

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
