#pragma once

#include <cstddef>

#include "imgui.h"

namespace FontConfig
{
	struct BuiltinFont
	{
		const char *name;
		const char *path;
	};

	struct FontSet
	{
		ImFont *interface_font = nullptr;
		ImFont *ascii_font = nullptr;
	};

	inline constexpr BuiltinFont builtin_ascii_fonts[] = {
		{"Menlo",
		 "/System/Library/Fonts/Menlo.ttc"},
		{"Monaco",
		 "/System/Library/Fonts/Monaco.ttf"},
		{"Courier New",
		 "/System/Library/Fonts/Supplemental/Courier New.ttf"},
		{"Andale Mono",
		 "/System/Library/Fonts/Supplemental/Andale Mono.ttf"}};

	inline constexpr std::size_t builtin_ascii_font_count = sizeof(builtin_ascii_fonts) / sizeof(builtin_ascii_fonts[0]);

	inline constexpr const char *interface_font_path = "/System/Library/Fonts/Supplemental/Courier New.ttf";

	inline constexpr float interface_font_size = 14.0f;

	inline constexpr ImWchar interface_glyph_ranges[] = {
		0x0020, 0x00FF,
		0};

	inline constexpr ImWchar ascii_glyph_ranges[] = {
		0x0020, 0x00FF, // Basic Latin and Latin Supplement
		0x2500, 0x257F, // Box Drawing
		0x2580, 0x259F, // Block Elements
		0};

	FontSet rebuild_font_atlas(
		const BuiltinFont &ascii_font_definition,
		float ascii_font_size);
}
