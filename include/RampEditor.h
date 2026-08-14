#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "imgui.h"

namespace AsciiArt::RampEditor
{
	struct State
	{
		std::array<char, 1024> buffer{};
		std::size_t cursor_byte = 0;
	};

	void reset(State &state, std::string_view ramp);
	bool insert(State &state, std::string_view character);
	void reverse(State &state);
	int capture_cursor(ImGuiInputTextCallbackData *data);
}
