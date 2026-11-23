#include "tv/input.hpp"

#include <utility>

namespace raspberry_tv {
namespace {
constexpr std::uint32_t InitialDelayMs = 350;
constexpr std::uint32_t RepeatEveryMs = 120;

std::size_t index(Button button) { return static_cast<std::size_t>(button); }
Action oneShot(Button button) {
    switch (button) {
        case Button::Cross: return Action::Confirm;
        case Button::Circle: return Action::Back;
        case Button::PS: return Action::Home;
        case Button::Options: return Action::Menu;
        case Button::L1: return Action::PreviousCategory;
        case Button::R1: return Action::NextCategory;
        case Button::L2: return Action::PreviousChannel;
        case Button::R2: return Action::NextChannel;
        case Button::Triangle: return Action::Favorites;
        case Button::Square: return Action::Details;
        default: return Action::None;
    }
}
} // namespace

void ControllerInput::onButton(Button button, bool pressed, std::uint32_t nowMs) {
    const auto i = index(button);
    if (i >= held_.size() || held_[i] == pressed) return;
    held_[i] = pressed;
    if (i <= index(Button::DpadRight)) {
        syncDirection(nowMs);
    } else if (pressed) {
        const Action action = oneShot(button);
        if (action != Action::None) pending_.push_back(action);
    }
}

int ControllerInput::filterAnalog(std::int16_t value, int previous) {
    // Activation threshold 18k, release threshold 11k: prevents jitter.
    if (value >= 18000) return 1;
    if (value <= -18000) return -1;
    if (value >= -11000 && value <= 11000) return 0;
    return previous;
}

void ControllerInput::onAxis(Axis axis, std::int16_t value, std::uint32_t nowMs) {
    switch (axis) {
        case Axis::LeftX:
            analogX_ = filterAnalog(value, analogX_);
            syncDirection(nowMs);
            break;
        case Axis::LeftY:
            analogY_ = filterAnalog(value, analogY_);
            syncDirection(nowMs);
            break;
        case Axis::TriggerLeft:
            onButton(Button::L2, value > 20000 || (held_[index(Button::L2)] && value > 12000), nowMs);
            break;
        case Axis::TriggerRight:
            onButton(Button::R2, value > 20000 || (held_[index(Button::R2)] && value > 12000), nowMs);
            break;
    }
}

void ControllerInput::syncDirection(std::uint32_t nowMs) {
    Action next = Action::None;
    // Physical D-pad wins over analog stick; they never trigger duplicates.
    if (held_[index(Button::DpadUp)]) next = Action::Up;
    else if (held_[index(Button::DpadDown)]) next = Action::Down;
    else if (held_[index(Button::DpadLeft)]) next = Action::Left;
    else if (held_[index(Button::DpadRight)]) next = Action::Right;
    else if (analogY_ < 0) next = Action::Up;
    else if (analogY_ > 0) next = Action::Down;
    else if (analogX_ < 0) next = Action::Left;
    else if (analogX_ > 0) next = Action::Right;
    if (next == direction_) return;
    direction_ = next;
    nextRepeatMs_ = nowMs + InitialDelayMs;
    if (next != Action::None) pending_.push_back(next);
}

std::vector<Action> ControllerInput::poll(std::uint32_t nowMs) {
    if (direction_ != Action::None && static_cast<std::int32_t>(nowMs - nextRepeatMs_) >= 0) {
        pending_.push_back(direction_);
        nextRepeatMs_ = nowMs + RepeatEveryMs;
    }
    std::vector<Action> actions;
    actions.swap(pending_);
    return actions;
}

void ControllerInput::disconnect() {
    held_.fill(false);
    analogX_ = 0;
    analogY_ = 0;
    direction_ = Action::None;
    nextRepeatMs_ = 0;
    pending_.clear();
}

const char* actionName(Action action) {
    switch (action) {
        case Action::None: return "none";
        case Action::Up: return "up";
        case Action::Down: return "down";
        case Action::Left: return "left";
        case Action::Right: return "right";
        case Action::Confirm: return "confirm";
        case Action::Back: return "back";
        case Action::Home: return "home";
        case Action::Menu: return "menu";
        case Action::PreviousCategory: return "previous-category";
        case Action::NextCategory: return "next-category";
        case Action::PreviousChannel: return "previous-channel";
        case Action::NextChannel: return "next-channel";
        case Action::Favorites: return "favorites";
        case Action::Details: return "details";
    }
    return "unknown";
}
} // namespace raspberry_tv
