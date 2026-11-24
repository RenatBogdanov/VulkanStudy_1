#include "Gamepad.h"

#include <Windows.h>
#include <Xinput.h>

#pragma comment(lib, "XInput.lib")

#include <cmath>

Gamepad::Gamepad() {
    // Первичная инициализация не требуется, XInputGetState вызывается в update()
}

void Gamepad::update() {
    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));

    // Опрос геймпада с индексом 0
    DWORD result = XInputGetState(0, &state);
    if (result != ERROR_SUCCESS) {
        if (m_connected) {
            // Геймпад был отключен
            m_connected = false;
            m_leftStick = glm::vec2(0.0f);
            m_rightStick = glm::vec2(0.0f);
            m_leftTrigger = 0.0f;
            m_rightTrigger = 0.0f;
        }
        return;
    }

    m_connected = true;

    // Обработка стиков с учетом мертвых зон
    m_leftStick = applyDeadzone(
        static_cast<float>(state.Gamepad.sThumbLX),
        static_cast<float>(state.Gamepad.sThumbLY),
        static_cast<float>(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
    );

    m_rightStick = applyDeadzone(
        static_cast<float>(state.Gamepad.sThumbRX),
        static_cast<float>(state.Gamepad.sThumbRY),
        static_cast<float>(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
    );

    // Обработка триггеров
    m_leftTrigger = (state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
        ? (static_cast<float>(state.Gamepad.bLeftTrigger) / 255.0f) : 0.0f;

    m_rightTrigger = (state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
        ? (static_cast<float>(state.Gamepad.bRightTrigger) / 255.0f) : 0.0f;
}

glm::vec2 Gamepad::applyDeadzone(float x, float y, float deadzone) {
    float magnitude = std::sqrt(x * x + y * y);

    if (magnitude > deadzone) {
        if (magnitude > 32767.0f) magnitude = 32767.0f;

        // Нормализация с учетом мертвой зоны
        float normalizedMagnitude = (magnitude - deadzone) / (32767.0f - deadzone);
        return glm::vec2(
            (x / magnitude) * normalizedMagnitude,
            (y / magnitude) * normalizedMagnitude
        );
    }
    return glm::vec2(0.0f);
}