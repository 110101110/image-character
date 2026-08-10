#pragma once

#include <cstddef>
#include <string>

#include <glad/glad.h>
#include "imgui.h"

namespace AsciiArt
{
	namespace Config
	{
		inline constexpr int window_width = 1600;
		inline constexpr int window_height = 900;
		inline constexpr float sidebar_width = 460.0f;
		inline constexpr const char *window_title = "ASCII Art";
		inline constexpr const char *glsl_version = "#version 410";

		inline constexpr int default_columns = 100;
		inline constexpr int default_rows = 50;
		inline constexpr float default_spacing_x = 7.0f;
		inline constexpr float default_spacing_y = 12.0f;
		inline constexpr float default_ascii_font_size = 14.0f;
		inline constexpr float minimum_ascii_font_size = 8.0f;
		inline constexpr float maximum_ascii_font_size = 64.0f;
		inline constexpr float default_export_scale = 2.0f;
		inline constexpr const char *default_ramp = " .:-=+*#%@";
	}

	struct ImageBuffer
	{
		unsigned char *pixels = nullptr;
		int width = 0;
		int height = 0;
		int channels = 0;
		GLuint textureID = 0;
	};

	struct AsciiOutput
	{
		std::string text;
		int cols = 0;
		int rows = 0;
	};

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

	struct SymbolGroup
	{
		const char *name;
		const char *symbols;
	};

	inline constexpr BuiltinFont builtin_ascii_fonts[] = {
		{"Menlo", "/System/Library/Fonts/Menlo.ttc"},
		{"Monaco", "/System/Library/Fonts/Monaco.ttf"},
		{"Courier New", "/System/Library/Fonts/Supplemental/Courier New.ttf"},
		{"Andale Mono", "/System/Library/Fonts/Supplemental/Andale Mono.ttf"}};

	inline constexpr std::size_t builtin_ascii_font_count =
		sizeof(builtin_ascii_fonts) / sizeof(builtin_ascii_fonts[0]);

	inline constexpr const char *interface_font_path =
		"/System/Library/Fonts/Supplemental/Courier New.ttf";
	inline constexpr float interface_font_size = 14.0f;
	inline constexpr const char *cjk_fallback_font_path = "/System/Library/Fonts/PingFang.ttc";
	inline constexpr const char *symbol_fallback_font_path = "/System/Library/Fonts/Apple Symbols.ttf";
	inline constexpr const char *dingbat_fallback_font_path = "/System/Library/Fonts/ZapfDingbats.ttf";

	inline constexpr ImWchar base_glyph_ranges[] = {
		0x0020, 0x00FF,
		0x2500, 0x259F,
		0};

	inline constexpr ImWchar cjk_glyph_ranges[] = {
		0x3000, 0x30FF, // CJK punctuation, Hiragana and Katakana
		0x3400, 0x4DBF, // CJK Unified Ideographs Extension A
		0x4E00, 0x9FFF, // CJK Unified Ideographs
		0xFF00, 0xFFEF, // Half-width and full-width forms
		0};

	inline constexpr ImWchar symbol_glyph_ranges[] = {
		0x2600, 0x26FF,
		0x2800, 0x28FF,
		0};

	inline constexpr ImWchar dingbat_glyph_ranges[] = {
		0x2700, 0x27BF,
		0};

	inline constexpr SymbolGroup special_symbol_groups[] = {
		{"Stars", "✧✦✩✪✫✬✭✮✯✰"},
		{"Flowers", "✿❀❁❂❃❉❊❋"},
		{"Katakana", "アカサタナハマヤラワ"},
		{"Braille", "⠁⠂⠄⡀⢀⠐⠠⡁⡂⡄⡈⡐⡠"},
		{"Blocks", "█▓▒░"}};

	inline constexpr std::size_t special_symbol_group_count =
		sizeof(special_symbol_groups) / sizeof(special_symbol_groups[0]);

	AsciiOutput generate_ascii(const ImageBuffer &image, int target_columns, int target_rows,
		float brightness, float contrast, bool invert, const std::string &ramp);

	std::string reverse_utf8(const std::string &text);

	int calculate_locked_rows(
		const ImageBuffer &image,
		int columns,
		float spacing_x,
		float spacing_y);

	int calculate_locked_columns(
		const ImageBuffer &image,
		int rows,
		float spacing_x,
		float spacing_y);

	float calculate_locked_spacing_y(
		const ImageBuffer &image,
		int columns,
		int rows,
		float spacing_x);

	float calculate_locked_spacing_x(
		const ImageBuffer &image,
		int columns,
		int rows,
		float spacing_y);

	GLuint create_texture_from_pixels(const unsigned char *pixels, int width, int height, int channels);

	void destroy_image_buffer(ImageBuffer &image);

	FontSet rebuild_font_atlas(const BuiltinFont &ascii_font_definition, float ascii_font_size);

	bool export_to_image(
		const std::string &filepath,
		const AsciiOutput &ascii,
		ImFont *ascii_font,
		float ascii_font_size,
		float spacing_x,
		float spacing_y,
		ImVec4 background_color,
		ImVec4 text_color,
		float glitch_intensity,
		bool export_full_canvas,
		ImVec2 viewport_size,
		float export_scale = Config::default_export_scale);

	bool export_to_text(
		const std::string &filepath,
		const AsciiOutput &ascii);
}
