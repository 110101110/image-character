#pragma once

#include <string_view>
#include <vector>

#include "TextLayout.h"

namespace AsciiArt::TextEditor
{
	struct State
	{
		bool enabled = false;
		bool modified = false;
		bool soft_wrap = true;
		bool layout_dirty = true;
		float width = 800.0f;
		int cached_wrap_columns = 0;
		std::vector<char> buffer = std::vector<char>(1024 * 1024, '\0');
		Text::Layout layout;
	};

	void set_text(State &editor, std::string_view text, bool mark_modified = false);
	void invalidate_layout(State &editor);
	const Text::Layout &update_layout(State &editor, int wrap_columns);
}
