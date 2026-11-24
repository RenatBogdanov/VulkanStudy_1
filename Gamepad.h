#pragma once

#include <glm/glm.hpp>

class Gamepad {
public:
    Gamepad();
    ~Gamepad() = default;

    void update();

    glm::vec2 getLeftStick() const { return m_leftStick; }
    glm::vec2 getRightStick() const { return m_rightStick; }
    float getLeftTrigger() const { return m_leftTrigger; }
    float getRightTrigger() const { return m_rightTrigger; }
    bool isConnected() const { return m_connected; }

private:
    bool m_connected = false;
    glm::vec2 m_leftStick = glm::vec2(0.0f);
    glm::vec2 m_rightStick = glm::vec2(0.0f);
    float m_leftTrigger = 0.0f;
    float m_rightTrigger = 0.0f;

    glm::vec2 applyDeadzone(float x, float y, float deadzone);
};