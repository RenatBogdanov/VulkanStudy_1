#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : m_position(position)
    , m_front(glm::vec3(0.0f, 0.0f, -1.0f))
    , m_worldUp(up)
    , m_yaw(yaw)
    , m_pitch(pitch)
    , m_speed(5.0f)
    , m_sensitivity(0.1f) {
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

void Camera::processKeyboard(GLFWwindow* window, float deltaTime) {
    float velocity = m_speed * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        m_position += m_front * velocity;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        m_position -= m_front * velocity;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        m_position -= m_right * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        m_position += m_right * velocity;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        m_position += m_worldUp * velocity;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        m_position -= m_worldUp * velocity;
}

void Camera::processMouseMovement(float xOffset, float yOffset) {
    m_yaw += xOffset * m_sensitivity;
    m_pitch += yOffset * m_sensitivity;

    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;

    updateCameraVectors();
}

void Camera::processGamepad(const glm::vec2& leftStick, const glm::vec2& rightStick, float leftTrigger, float rightTrigger, float deltaTime) {
    float velocity = m_speed * deltaTime;

    m_position += m_front * (leftStick.y * velocity);
    m_position += m_right * (leftStick.x * velocity);
    m_position += m_worldUp * ((rightTrigger - leftTrigger) * velocity);

    float rotSpeed = 120.0f * deltaTime;
    m_yaw += rightStick.x * rotSpeed;
    m_pitch += rightStick.y * rotSpeed;

    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;

    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

    m_front = glm::normalize(front);
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}

void Camera::setSpeed(float speed) {
    m_speed = speed;
}

void Camera::reset() {
    m_position = glm::vec3(0.0f, 0.0f, 3.0f);
    m_yaw = -90.0f;
    m_pitch = 0.0f;
    updateCameraVectors();
}