#pragma once

#include <cstddef>
#include <string>

#include <glad/glad.h>
#include "imgui.h"

namespace AsciiArt
{
	namespace Config
	{
		inline constexpr int window_width = 1400;
		inline constexpr int window_height = 850;
		inline constexpr int sidebar_width = 360;
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

	inline constexpr ImWchar interface_glyph_ranges[] = {
		0x0020, 0x00FF,
		0};

	inline constexpr ImWchar ascii_glyph_ranges[] = {
		0x0020, 0x00FF, // Basic Latin and Latin Supplement
		0x2500, 0x257F, // Box Drawing
		0x2580, 0x259F, // Block Elements
		0};

	AsciiOutput generate_ascii(const ImageBuffer &image, int target_columns, int target_rows,
		float brightness, float contrast, bool invert, const std::string &ramp);

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

	bool export_to_text();
}
