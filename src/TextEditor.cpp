#include "TextEditor.h"

#include <algorithm>
#include <cstring>

namespace AsciiArt::TextEditor
{
	void set_text(State &editor, std::string_view text, bool mark_modified)
	{
		const std::size_t copy_size = std::min(text.size(), editor.buffer.size() - 1);
		std::memcpy(editor.buffer.data(), text.data(), copy_size);
		editor.buffer[copy_size] = '\0';
		editor.modified = mark_modified;
		editor.layout_dirty = true;
	}

	void invalidate_layout(State &editor)
	{
		editor.layout_dirty = true;
	}

	const Text::Layout &update_layout(State &editor, int wrap_columns)
	{
		wrap_columns = std::max(1, wrap_columns);
		if (editor.layout_dirty || editor.cached_wrap_columns != wrap_columns)
		{
			editor.layout = Text::build_layout(
				editor.buffer.data(), editor.soft_wrap, wrap_columns);
			editor.cached_wrap_columns = wrap_columns;
			editor.layout_dirty = false;
		}
		return editor.layout;
	}
}
