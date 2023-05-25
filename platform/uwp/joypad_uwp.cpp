/**************************************************************************/
/*  joypad_uwp.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "joypad_uwp.h"

#include "core/os/os.h"

using namespace winrt;
using namespace winrt::Windows::Gaming::Input;
using namespace winrt::Windows::Foundation;

void JoypadUWP::register_events() {
	Gamepad::GamepadAdded({ this, &JoypadUWP::OnGamepadAdded });
	Gamepad::GamepadRemoved({ this, &JoypadUWP::OnGamepadRemoved });
}

void JoypadUWP::process_controllers() {
	for (int i = 0; i < MAX_CONTROLLERS; i++) {
		ControllerDevice &joy = controllers[i];

		if (!joy.connected) {
			break;
		}

		switch (joy.type) {
			case ControllerType::GAMEPAD_CONTROLLER: {
				GamepadReading reading = joy.controller_reference.try_as<Gamepad>().GetCurrentReading();

				int button_mask = 1;
				for (int j = 0; j < 19; j++) {
					input->joy_button(joy.id, JoyButton(j), (int)reading.Buttons & button_mask);
					button_mask *= 2;
				}

				input->joy_axis(joy.id, JoyAxis::LEFT_X, axis_correct(reading.LeftThumbstickX));
				input->joy_axis(joy.id, JoyAxis::LEFT_Y, axis_correct(reading.LeftThumbstickY, true));
				input->joy_axis(joy.id, JoyAxis::RIGHT_X, axis_correct(reading.RightThumbstickX));
				input->joy_axis(joy.id, JoyAxis::RIGHT_Y, axis_correct(reading.RightThumbstickY, true));
				input->joy_axis(joy.id, JoyAxis::TRIGGER_LEFT, axis_correct(reading.LeftTrigger, false, true));
				input->joy_axis(joy.id, JoyAxis::TRIGGER_RIGHT, axis_correct(reading.RightTrigger, false, true));

				uint64_t timestamp = input->get_joy_vibration_timestamp(joy.id);
				if (timestamp > joy.ff_timestamp) {
					Vector2 strength = input->get_joy_vibration_strength(joy.id);
					float duration = input->get_joy_vibration_duration(joy.id);
					if (strength.x == 0 && strength.y == 0) {
						joypad_vibration_stop(i, timestamp);
					} else {
						joypad_vibration_start(i, strength.x, strength.y, duration, timestamp);
					}
				} else if (joy.vibrating && joy.ff_end_timestamp != 0) {
					uint64_t current_time = OS::get_singleton()->get_ticks_usec();
					if (current_time >= joy.ff_end_timestamp) {
						joypad_vibration_stop(i, current_time);
					}
				}

				break;
			}
		}
	}
}

JoypadUWP::JoypadUWP() {
	for (int i = 0; i < MAX_CONTROLLERS; i++) {
		controllers[i].id = i;
	}

	input = Input::get_singleton();
}

void JoypadUWP::OnGamepadAdded(IInspectable sender, Gamepad value) {
	short idx = -1;

	for (int i = 0; i < MAX_CONTROLLERS; i++) {
		if (!controllers[i].connected) {
			idx = i;
			break;
		}
	}

	ERR_FAIL_COND(idx == -1);

	controllers[idx].connected = true;
	controllers[idx].controller_reference = value;
	controllers[idx].id = idx;
	controllers[idx].type = ControllerType::GAMEPAD_CONTROLLER;

	input->joy_connection_changed(controllers[idx].id, true, "Xbox Controller", "__UWP_GAMEPAD__");
}

void JoypadUWP::OnGamepadRemoved(IInspectable sender, Gamepad value) {
	short idx = -1;

	for (int i = 0; i < MAX_CONTROLLERS; i++) {
		if (controllers[i].controller_reference == value) {
			idx = i;
			break;
		}
	}

	ERR_FAIL_COND(idx == -1);

	controllers[idx] = ControllerDevice();

	input->joy_connection_changed(idx, false, "Xbox Controller");
}

JoyButton JoypadUWP::gamepad_button_to_joy_button(GamepadButtons p_button) {
	switch (p_button) {
		case GamepadButtons::None:
			return JoyButton::INVALID;
		case GamepadButtons::Menu:
			return JoyButton::START;
		case GamepadButtons::View:
			return JoyButton::BACK;
		case GamepadButtons::A:
			return JoyButton::A;
		case GamepadButtons::B:
			return JoyButton::B;
		case GamepadButtons::X:
			return JoyButton::X;
		case GamepadButtons::Y:
			return JoyButton::Y;
		case GamepadButtons::DPadUp:
			return JoyButton::DPAD_UP;
		case GamepadButtons::DPadDown:
			return JoyButton::DPAD_DOWN;
		case GamepadButtons::DPadLeft:
			return JoyButton::DPAD_LEFT;
		case GamepadButtons::DPadRight:
			return JoyButton::DPAD_RIGHT;
		case GamepadButtons::LeftShoulder:
			return JoyButton::LEFT_SHOULDER;
		case GamepadButtons::RightShoulder:
			return JoyButton::RIGHT_SHOULDER;
		case GamepadButtons::LeftThumbstick:
			return JoyButton::LEFT_STICK;
		case GamepadButtons::RightThumbstick:
			return JoyButton::RIGHT_STICK;
		case GamepadButtons::Paddle1:
			return JoyButton::PADDLE1;
		case GamepadButtons::Paddle2:
			return JoyButton::PADDLE2;
		case GamepadButtons::Paddle3:
			return JoyButton::PADDLE3;
		case GamepadButtons::Paddle4:
			return JoyButton::PADDLE4;
	}
	return JoyButton::INVALID;
}

float JoypadUWP::axis_correct(double p_val, bool p_negate, bool p_trigger) const {
	if (p_trigger) {
		// Convert to a value between -1.0f and 1.0f.
		return 2.0f * p_val - 1.0f;
	}
	return (float)(p_negate ? -p_val : p_val);
}

void JoypadUWP::joypad_vibration_start(int p_device, float p_weak_magnitude, float p_strong_magnitude, float p_duration, uint64_t p_timestamp) {
	ControllerDevice &joy = controllers[p_device];
	if (joy.connected) {
		GamepadVibration vibration;
		vibration.LeftMotor = p_strong_magnitude;
		vibration.RightMotor = p_weak_magnitude;
		joy.controller_reference.try_as<Gamepad>().Vibration(vibration);

		joy.ff_timestamp = p_timestamp;
		joy.ff_end_timestamp = p_duration == 0 ? 0 : p_timestamp + (uint64_t)(p_duration * 1000000.0);
		joy.vibrating = true;
	}
}

void JoypadUWP::joypad_vibration_stop(int p_device, uint64_t p_timestamp) {
	ControllerDevice &joy = controllers[p_device];
	if (joy.connected) {
		GamepadVibration vibration;
		vibration.LeftMotor = 0.0;
		vibration.RightMotor = 0.0;
		joy.controller_reference.try_as<Gamepad>().Vibration(vibration);

		joy.ff_timestamp = p_timestamp;
		joy.vibrating = false;
	}
}
