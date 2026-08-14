#include "TextLayout.h"

#include <algorithm>

#include "imgui_internal.h"

namespace AsciiArt::Text
{
	Layout build_layout(std::string_view text, bool soft_wrap, int wrap_columns)
	{
		Layout layout;
		if (text.empty())
			return layout;

		wrap_columns = std::max(1, wrap_columns);
		const char *begin = text.data();
		const char *cursor = begin;
		const char *end = begin + text.size();
		int column = 0;
		int source_column = 0;
		int row = 0;
		bool last_was_newline = false;

		while (cursor < end)
		{
			unsigned int codepoint = 0;
			const int byte_count = ImTextCharFromUtf8(&codepoint, cursor, end);
			if (byte_count <= 0)
			{
				++cursor;
				continue;
			}

			const std::size_t byte_offset = static_cast<std::size_t>(cursor - begin);
			cursor += byte_count;
			if (codepoint == '\r')
				continue;
			if (codepoint == '\n')
			{
				last_was_newline = true;
				layout.output.text.push_back('\n');
				layout.output.cols = std::max(layout.output.cols, column);
				layout.maximum_source_columns = std::max(
					layout.maximum_source_columns, source_column);
				column = 0;
				source_column = 0;
				++row;
				continue;
			}
			last_was_newline = false;

			if (soft_wrap && column >= wrap_columns)
			{
				layout.output.text.push_back('\n');
				layout.output.cols = std::max(layout.output.cols, column);
				column = 0;
				++row;
			}

			layout.glyphs.push_back({codepoint, byte_offset, static_cast<std::size_t>(byte_count), column, row});
			layout.output.text.append(text.substr(byte_offset, byte_count));
			++column;
			++source_column;
		}

		layout.output.cols = std::max(layout.output.cols, column);
		layout.output.rows = row + (last_was_newline ? 0 : 1);
		layout.maximum_source_columns = std::max(layout.maximum_source_columns, source_column);
		return layout;
	}
}
