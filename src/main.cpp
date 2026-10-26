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
#include <cstdlib>
#include <cmath>

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

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;
const int MODEL_INPUT_SIZE = 640;

const std::vector<std::string> CLASS_LABELS = {
    "air", "dirrection", "earth", "fire", "temp", "unknown", "water"
};

struct Player {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    float cameraHeight = 1.6f;
    bool isGrounded = true;
    bool isCrouching = false;
    const float MAX_GROUND_SPEED = 8.0f;
    const float MAX_CROUCH_SPEED = 4.0f;
    const float GROUND_ACCEL     = 14.0f;
    const float GROUND_FRICTION  = 6.0f;
    const float AIR_ACCEL        = 45.0f;
    const float JUMP_IMPULSE     = 6.5f;
    const float GRAVITY          = 20.0f;
} player;

struct CanvasState {
    bool isOpen = false;
    float slideAnim = 0.0f;
    bool isDrawing = false;
    std::vector<std::vector<glm::vec2>> strokes;
    std::vector<glm::vec2> currentStroke;
    glm::vec2 shakeOffset = glm::vec2(0.0f);
    glm::vec2 currentTotalOffset = glm::vec2(0.0f);
} canvas;

glm::vec3 cameraFront = glm::vec3(0.0f, -0.2f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw   = -90.0f;
float pitch =  0.0f;
float lastX =  SCR_WIDTH / 2.0f;
float lastY =  SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --- DEBUG BMP EXPORTER ---
void SaveDebugBMP(const std::vector<float>& tensor, const std::string& filename) {
    int w = MODEL_INPUT_SIZE, h = MODEL_INPUT_SIZE;
    int filesize = 54 + 3 * w * h;

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

    int planeSize = w * h;
    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            unsigned char r = static_cast<unsigned char>(std::clamp(tensor[0 * planeSize + idx] * 255.0f, 0.0f, 255.0f));
            unsigned char g = static_cast<unsigned char>(std::clamp(tensor[1 * planeSize + idx] * 255.0f, 0.0f, 255.0f));
            unsigned char b = static_cast<unsigned char>(std::clamp(tensor[2 * planeSize + idx] * 255.0f, 0.0f, 255.0f));
            f.write((char*)&b, 1);
            f.write((char*)&g, 1);
            f.write((char*)&r, 1);
        }
    }
    f.close();
    std::cout << " 📸 Saved debug canvas snapshot to: '" << filename << "'" << std::endl;
}

// --- STROKE RASTERIZER WITH WHITE BACKGROUND & THICK BRUSH ---
std::vector<float> RasterizeAndCropStrokes(const std::vector<std::vector<glm::vec2>>& strokes, bool invertColors = true) {
    int planeSize = MODEL_INPUT_SIZE * MODEL_INPUT_SIZE;
    
    // Default background color: 1.0 (White) if invertColors else 0.0 (Black)
    float bgVal = invertColors ? 1.0f : 0.0f;
    float strokeVal = invertColors ? 0.0f : 1.0f;

    std::vector<float> tensor(1 * 3 * planeSize, bgVal);

    if (strokes.empty()) return tensor;

    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    bool hasPoints = false;

    for (const auto& stroke : strokes) {
        for (const auto& pt : stroke) {
            minX = std::min(minX, pt.x);
            maxX = std::max(maxX, pt.x);
            minY = std::min(minY, pt.y);
            maxY = std::max(maxY, pt.y);
            hasPoints = true;
        }
    }

    if (!hasPoints) return tensor;

    float drawWidth = std::max(maxX - minX, 1.0f);
    float drawHeight = std::max(maxY - minY, 1.0f);

    float padding = 60.0f;
    float targetSize = MODEL_INPUT_SIZE - (2.0f * padding);
    float scale = std::min(targetSize / drawWidth, targetSize / drawHeight);

    float offsetX = padding + (targetSize - drawWidth * scale) * 0.5f;
    float offsetY = padding + (targetSize - drawHeight * scale) * 0.5f;

    auto drawThickPixel = [&](int cx, int cy, int radius = 8) {
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
            drawThickPixel(x0, y0, 8);
            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    };

    for (const auto& stroke : strokes) {
        if (stroke.size() < 2) continue;
        for (size_t i = 0; i < stroke.size() - 1; ++i) {
            int x0 = static_cast<int>((stroke[i].x - minX) * scale + offsetX);
            int y0 = static_cast<int>((stroke[i].y - minY) * scale + offsetY);
            int x1 = static_cast<int>((stroke[i+1].x - minX) * scale + offsetX);
            int y1 = static_cast<int>((stroke[i+1].y - minY) * scale + offsetY);
            drawLine(x0, y0, x1, y1);
        }
    }

    return tensor;
}

struct GestureResult {
    int classId;
    std::string label;
    float confidence;
};

GestureResult EvaluateGesture(const std::vector<float>& inputTensor) {
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
        int numClasses = 7;
        int bestClass = 5;
        float highestConf = 0.0f;

        for (int cls = 0; cls < numClasses; ++cls) {
            int channelIdx = 4 + cls;
            for (int anchor = 0; anchor < numAnchors; ++anchor) {
                float score = rawData[channelIdx * numAnchors + anchor];
                if (score > highestConf) {
                    highestConf = score;
                    bestClass = cls;
                }
            }
        }

        std::string label = (bestClass >= 0 && bestClass < (int)CLASS_LABELS.size()) ? CLASS_LABELS[bestClass] : "unknown";
        return {bestClass, label, highestConf * 100.0f};

    } catch (const std::exception& e) {
        std::cerr << " [ONNX ERROR] " << e.what() << std::endl;
        return {5, "unknown", 0.0f};
    }
#else
    return {5, "unknown", 0.0f};
#endif
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (canvas.isOpen) {
        if (canvas.isDrawing) {
            glm::vec2 localPos = glm::vec2(xpos, ypos) - canvas.currentTotalOffset;
            canvas.currentStroke.push_back(localPos);
        }
        return;
    }

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

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (!canvas.isOpen) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            canvas.isDrawing = true;
            canvas.currentStroke.clear();
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            glm::vec2 localPos = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos)) - canvas.currentTotalOffset;
            canvas.currentStroke.push_back(localPos);
        } else if (action == GLFW_RELEASE) {
            if (canvas.isDrawing) {
                canvas.isDrawing = false;
                if (!canvas.currentStroke.empty()) {
                    canvas.strokes.push_back(canvas.currentStroke);
                    canvas.currentStroke.clear();
                }
            }
        }
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_J || key == GLFW_KEY_P) {
            if (canvas.isOpen) {
                std::cout << "\n=========================================" << std::endl;
                std::cout << "🔮 CASTING SPELL GESTURE (" << canvas.strokes.size() << " strokes)..." << std::endl;
                
                // Mode A: White background / Black stroke (Standard dataset format)
                std::vector<float> inputTensor = RasterizeAndCropStrokes(canvas.strokes, true);
                SaveDebugBMP(inputTensor, "debug_input.bmp");

                GestureResult result = EvaluateGesture(inputTensor);
                
                // If confidence is low, retry Mode B: Black background / White stroke
                if (result.confidence < 25.0f) {
                    std::vector<float> inputTensorB = RasterizeAndCropStrokes(canvas.strokes, false);
                    GestureResult resultB = EvaluateGesture(inputTensorB);
                    if (resultB.confidence > result.confidence) {
                        result = resultB;
                        SaveDebugBMP(inputTensorB, "debug_input.bmp");
                    }
                }

                std::cout << "✨ SPELL RECOGNIZED : '" << result.label << "' (Class ID: " << result.classId << ")" << std::endl;
                std::cout << "🎯 CONFIDENCE SCORE : " << result.confidence << "%" << std::endl;
                std::cout << "=========================================\n" << std::endl;

                canvas.strokes.clear();
                canvas.currentStroke.clear();
                canvas.isOpen = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            } else {
                canvas.isOpen = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
    }
}

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
    player.isCrouching = (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS);
    float targetCameraHeight = player.isCrouching ? 0.8f : 1.6f;
    player.cameraHeight = glm::mix(player.cameraHeight, targetCameraHeight, 12.0f * dt);

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

    if (player.isGrounded) {
        ApplyFriction(dt);
        if (glm::length(wishdir) > 0.001f) {
            Accelerate(wishdir, targetMaxSpeed, player.GROUND_ACCEL, dt);
        }
        
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            player.velocity.y = player.JUMP_IMPULSE;
            player.isGrounded = false;
        }
    } else {
        if (glm::length(wishdir) > 0.001f) {
            Accelerate(wishdir, targetMaxSpeed, player.AIR_ACCEL, dt);
        }
        player.velocity.y -= player.GRAVITY * dt;
    }

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
    std::cout << "🚀 WitchGE: Native Drawing Recognition Active" << std::endl;
    std::cout << "==================================================" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        UpdatePlayerPhysics(window, deltaTime);

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

        // --- 1. RENDER 3D SCENE ---
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader3D);

        glm::vec3 cameraPos = player.position + glm::vec3(0.0f, player.cameraHeight, 0.0f);
        glm::mat4 projection3D = glm::perspective(glm::radians(65.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 150.0f);
        glm::mat4 view3D = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 model3D = glm::mat4(1.0f);

        glUniformMatrix4fv(glGetUniformLocation(shader3D, "model"), 1, GL_FALSE, glm::value_ptr(model3D));
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "view"), 1, GL_FALSE, glm::value_ptr(view3D));
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "projection"), 1, GL_FALSE, glm::value_ptr(projection3D));

        glBindVertexArray(VAO3D);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(gridVertices.size() / 6));

        // --- 2. RENDER 2D UI SLATE & STROKES ---
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
