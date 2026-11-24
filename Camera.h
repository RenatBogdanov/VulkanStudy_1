#pragma once

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
        float yaw = -90.0f, float pitch = 0.0f);

    glm::mat4 getViewMatrix() const;
    void processKeyboard(GLFWwindow* window, float deltaTime);
    void processMouseMovement(float xOffset, float yOffset);

    // Новый метод для обработки ввода с геймпада
    void processGamepad(const glm::vec2& leftStick, const glm::vec2& rightStick, float leftTrigger, float rightTrigger, float deltaTime);

    glm::vec3 getPosition() const { return m_position; }

private:
    void updateCameraVectors();

    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;
    glm::vec3 m_worldUp;

    float m_yaw;
    float m_pitch;

    float m_speed;
    float m_sensitivity;
};