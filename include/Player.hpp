#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

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

    void update(GLFWwindow* window, float dt, const glm::vec3& cameraFront, const glm::vec3& cameraUp);

private:
    void applyFriction(float dt);
    void accelerate(glm::vec3 wishdir, float wishspeed, float accel, float dt);
};

#endif // PLAYER_HPP