#include "RampEditor.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "imgui_internal.h"

namespace AsciiArt::RampEditor
{
	void reset(State &state, std::string_view ramp)
	{
		const std::size_t size = std::min(ramp.size(), state.buffer.size() - 1);
		std::memcpy(state.buffer.data(), ramp.data(), size);
		state.buffer[size] = '\0';
		state.cursor_byte = size;
	}

	bool insert(State &state, std::string_view character)
	{
		const std::size_t current_size = std::strlen(state.buffer.data());
		state.cursor_byte = std::min(state.cursor_byte, current_size);
		if (current_size + character.size() >= state.buffer.size())
			return false;
		std::memmove(state.buffer.data() + state.cursor_byte + character.size(),
			state.buffer.data() + state.cursor_byte, current_size - state.cursor_byte + 1);
		std::memcpy(state.buffer.data() + state.cursor_byte, character.data(), character.size());
		state.cursor_byte += character.size();
		return true;
	}

	void reverse(State &state)
	{
		std::vector<std::string> characters;
		const char *cursor = state.buffer.data();
		const char *end = cursor + std::strlen(cursor);
		while (cursor < end)
		{
			unsigned int codepoint = 0;
			const int byte_count = ImTextCharFromUtf8(&codepoint, cursor, end);
			if (byte_count <= 0)
				break;
			characters.emplace_back(cursor, static_cast<std::size_t>(byte_count));
			cursor += byte_count;
		}
		std::reverse(characters.begin(), characters.end());
		std::string reversed;
		for (const std::string &character : characters)
			reversed += character;
		reset(state, reversed);
	}

	int capture_cursor(ImGuiInputTextCallbackData *data)
	{
		auto *state = static_cast<State *>(data->UserData);
		state->cursor_byte = static_cast<std::size_t>(std::max(0, data->CursorPos));
		return 0;
	}
}
