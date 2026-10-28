#include "Player.hpp"
#include <algorithm>

void Player::applyFriction(float dt) {
    float speed = glm::length(glm::vec2(velocity.x, velocity.z));
    if (speed < 0.0001f) {
        velocity.x = 0.0f;
        velocity.z = 0.0f;
        return;
    }
    float drop = speed * GROUND_FRICTION * dt;
    float newSpeed = std::max(0.0f, speed - drop);
    newSpeed /= speed;
    velocity.x *= newSpeed;
    velocity.z *= newSpeed;
}

void Player::accelerate(glm::vec3 wishdir, float wishspeed, float accel, float dt) {
    float currentspeed = glm::dot(velocity, wishdir);
    float addspeed = wishspeed - currentspeed;
    if (addspeed <= 0) return;
    float accelspeed = accel * dt * wishspeed;
    if (accelspeed > addspeed) accelspeed = addspeed;
    velocity += wishdir * accelspeed;
}

void Player::update(GLFWwindow* window, float dt, const glm::vec3& cameraFront, const glm::vec3& cameraUp) {
    isCrouching = (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS);
    float targetCameraHeight = isCrouching ? 0.8f : 1.6f;
    cameraHeight = glm::mix(cameraHeight, targetCameraHeight, 12.0f * dt);

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

    float targetMaxSpeed = isCrouching ? MAX_CROUCH_SPEED : MAX_GROUND_SPEED;

    if (isGrounded) {
        applyFriction(dt);
        if (glm::length(wishdir) > 0.001f) {
            accelerate(wishdir, targetMaxSpeed, GROUND_ACCEL, dt);
        }
        
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            velocity.y = JUMP_IMPULSE;
            isGrounded = false;
        }
    } else {
        if (glm::length(wishdir) > 0.001f) {
            accelerate(wishdir, targetMaxSpeed, AIR_ACCEL, dt);
        }
        velocity.y -= GRAVITY * dt;
    }

    position += velocity * dt;

    if (position.y <= 0.0f) {
        position.y = 0.0f;
        velocity.y = 0.0f;
        isGrounded = true;
    }
}