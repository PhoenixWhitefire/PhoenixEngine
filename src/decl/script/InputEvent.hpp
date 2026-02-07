#pragma once

#include <cstdint>

enum class InputEventType : uint8_t
{
	NONE,
	Key,
	MouseButton,
	Scroll
};

struct InputEvent
{
	union {
		struct {
			int Button = 0;
			int Scancode = 0;
			int Action = 0;
			int Modifiers = 0;
		} Key;
		struct {
			int Button = 0;
			int Action = 0;
			int Modifiers = 0;
		} MouseButton;
		struct {
			double XOffset = 0.0f;
			double YOffset = 0.0f;
		} Scroll;
	};

	InputEventType Type = InputEventType::NONE;
};
