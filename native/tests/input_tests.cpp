#include "tv/input.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace raspberry_tv;

void expect(ControllerInput& input, std::uint32_t now, std::initializer_list<Action> expected) {
    const auto actual = input.poll(now);
    if (actual != std::vector<Action>(expected)) {
        std::cerr << "Unexpected action count at " << now << "ms: got " << actual.size()
                  << ", expected " << expected.size() << "\n";
        std::exit(EXIT_FAILURE);
    }
}
int main() {
    ControllerInput in;
    // Action buttons are strictly edge-triggered.
    in.onButton(Button::Cross, true, 0); expect(in, 0, {Action::Confirm});
    in.onButton(Button::Cross, true, 10); expect(in, 10, {});
    expect(in, 20000, {}); // button held for 20 seconds
    in.onButton(Button::Cross, false, 20001); expect(in, 20001, {});
    in.onButton(Button::Cross, true, 20002); expect(in, 20002, {Action::Confirm});

    in.disconnect();
    in.onButton(Button::DpadUp, true, 100); expect(in, 100, {Action::Up});
    expect(in, 450, {}); expect(in, 570, {}); expect(in, 60000, {}); // no auto-repeat
    in.onButton(Button::DpadUp, true, 60001); expect(in, 60001, {});
    // Analog same direction cannot duplicate D-pad action.
    in.onAxis(Axis::LeftY, -23000, 60002); expect(in, 60002, {});
    expect(in, 60082, {});
    in.onButton(Button::DpadUp, false, 60083); expect(in, 60083, {});
    in.onAxis(Axis::LeftY, 0, 60084); expect(in, 60164, {});
    // New button press generates exactly one action.
    in.onButton(Button::DpadRight, true, 60170); expect(in, 60170, {Action::Right});
    expect(in, 90000, {});
    in.onButton(Button::DpadRight, false, 90001); expect(in, 90001, {});

    in.disconnect();
    // Small stick drift below activation threshold does not cause movement.
    in.onAxis(Axis::LeftX, 18000, 1000); expect(in, 2000, {});
    // Accept a stable intentional deflection only after 80 ms.
    in.onAxis(Axis::LeftX, 27000, 2200); expect(in, 2200, {});
    expect(in, 2279, {}); expect(in, 2280, {Action::Right});
    expect(in, 10000, {}); // held analog does not repeat
    in.onAxis(Axis::LeftX, 15000, 10001); expect(in, 10081, {}); // hysteresis
    // Short neutral jitter: cancelled before the 80-ms release.
    in.onAxis(Axis::LeftX, 0, 11000); expect(in, 11030, {});
    in.onAxis(Axis::LeftX, 27000, 11040); expect(in, 11130, {});
    // Genuine release, then a second deliberate movement.
    in.onAxis(Axis::LeftX, 0, 12000); expect(in, 12080, {});
    in.onAxis(Axis::LeftX, 27000, 13000); expect(in, 13080, {Action::Right});

    in.disconnect();
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
    // Reconnection starts a fresh controller state; queued old events disappear.
    in.onButton(Button::Cross,true,2100);
    in.disconnect();expect(in,2200,{});
    in.onButton(Button::Cross,true,2300);expect(in,2300,{Action::Confirm});
    in.disconnect();
    in.onAxis(Axis::TriggerLeft,25000,2400);expect(in,2400,{Action::PreviousChannel});
    in.disconnect();
    in.onAxis(Axis::TriggerLeft,25000,2500);expect(in,2500,{Action::PreviousChannel});
    // Stability arithmetic remains correct when SDL's millisecond counter wraps.
    in.disconnect();in.onAxis(Axis::LeftX,27000,0xfffffff0u);
    expect(in,0x20u,{});expect(in,0x40u,{Action::Right});
    std::cout << "ControllerInput no-repeat tests passed\n";
}
