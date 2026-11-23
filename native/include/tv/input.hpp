#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace raspberry_tv {

enum class Action : std::uint8_t {
    None, Up, Down, Left, Right, Confirm, Back, Home, Menu,
    PreviousCategory, NextCategory, PreviousChannel, NextChannel,
    Favorites, Details
};

enum class Button : std::uint8_t {
    DpadUp, DpadDown, DpadLeft, DpadRight,
    Cross, Circle, PS, Options, L1, R1, L2, R2,
    Triangle, Square, Count
};

enum class Axis : std::uint8_t { LeftX, LeftY, TriggerLeft, TriggerRight };

// Independent from SDL: controller events enter here after normalization.
// All input actions are edge-triggered except direction, which repeats while held.
class ControllerInput {
public:
    void onButton(Button button, bool pressed, std::uint32_t nowMs);
    void onAxis(Axis axis, std::int16_t value, std::uint32_t nowMs);
    std::vector<Action> poll(std::uint32_t nowMs);
    void disconnect();

private:
    static constexpr std::size_t ButtonCount = static_cast<std::size_t>(Button::Count);
    std::array<bool, ButtonCount> held_{};
    int analogX_ = 0;
    int analogY_ = 0;
    Action direction_ = Action::None;
    std::uint32_t nextRepeatMs_ = 0;
    std::vector<Action> pending_;

    void syncDirection(std::uint32_t nowMs);
    static int filterAnalog(std::int16_t value, int previous);
};

const char* actionName(Action action);

} // namespace raspberry_tv
