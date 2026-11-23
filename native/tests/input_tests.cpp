#include "tv/input.hpp"
#include <cassert>
#include <iostream>
#include <vector>
using namespace raspberry_tv;

void expect(ControllerInput& input, std::uint32_t now, std::initializer_list<Action> expected) {
    assert(input.poll(now) == std::vector<Action>(expected));
}
int main() {
    ControllerInput in;
    in.onButton(Button::Cross, true, 0); expect(in, 0, {Action::Confirm});
    in.onButton(Button::Cross, true, 10); expect(in, 10, {});
    in.onButton(Button::Cross, false, 11);
    in.onButton(Button::Cross, true, 12); expect(in, 12, {Action::Confirm});
    in.disconnect();
    in.onButton(Button::DpadUp, true, 100); expect(in, 100, {Action::Up});
    expect(in, 449, {}); expect(in, 450, {Action::Up}); expect(in, 570, {Action::Up});
    in.onAxis(Axis::LeftY, -23000, 600); expect(in, 600, {}); // no duplicate D-pad+analog
    in.onButton(Button::DpadUp, false, 610); expect(in, 610, {}); // stick still held
    in.onAxis(Axis::LeftY, 0, 620); expect(in, 620, {});
    in.onAxis(Axis::LeftX, 23000, 700); expect(in, 700, {Action::Right});
    in.onAxis(Axis::LeftX, 13000, 701); expect(in, 701, {}); // hysteresis
    in.onAxis(Axis::LeftX, 0, 702); expect(in, 702, {});
    in.onAxis(Axis::TriggerRight, 23000, 800); expect(in, 800, {Action::NextChannel});
    in.onAxis(Axis::TriggerRight, 16000, 801); expect(in, 801, {});
    in.onAxis(Axis::TriggerRight, 0, 802); expect(in, 802, {});
    in.onAxis(Axis::TriggerRight, 23000, 803); expect(in, 803, {Action::NextChannel});
    in.onButton(Button::PS, true, 810); expect(in, 810, {Action::Home});
    in.onButton(Button::Options, true, 820); expect(in, 820, {Action::Menu});
    in.onButton(Button::L1, true, 821); expect(in, 821, {Action::PreviousCategory});
    in.onButton(Button::Triangle, true, 822); expect(in, 822, {Action::Favorites});
    in.onButton(Button::Square, true, 823); expect(in, 823, {Action::Details});
    in.disconnect(); expect(in, 2000, {});
    std::cout << "ControllerInput tests passed\n";
}
