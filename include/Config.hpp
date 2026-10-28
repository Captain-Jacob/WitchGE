#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>

constexpr unsigned int SCR_WIDTH = 1280;
constexpr unsigned int SCR_HEIGHT = 720;
constexpr int MODEL_INPUT_SIZE = 640;

const std::vector<std::string> CLASS_LABELS = {
    "air", "dirrection", "earth", "fire", "temp", "unknown", "water"
};

#endif // CONFIG_HPP