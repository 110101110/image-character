#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "AsciiArt.h"

namespace AsciiArt::Text
{
	struct Glyph
	{
		unsigned int codepoint = 0;
		std::size_t byte_offset = 0;
		std::size_t byte_length = 0;
		int column = 0;
		int row = 0;
	};

	struct Layout
	{
		AsciiOutput output;
		std::vector<Glyph> glyphs;
		int maximum_source_columns = 0;
	};

	Layout build_layout(std::string_view text, bool soft_wrap, int wrap_columns);
}
