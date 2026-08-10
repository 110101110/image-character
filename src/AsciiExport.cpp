#include "AsciiArt.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "imgui_internal.h"
#include "stb_image_write.h"

namespace AsciiArt
{
	bool export_to_image(const std::string &filepath, const AsciiOutput &ascii, ImFont *ascii_font,
		float ascii_font_size, float spacing_x, float spacing_y, ImVec4 background_color, ImVec4 text_color,
		float glitch_intensity, bool export_full_canvas, ImVec2 viewport_size, float export_scale)
	{
		if (filepath.empty() || ascii.text.empty() || ascii.cols <= 0 || ascii.rows <= 0 ||
			!ascii_font || ascii_font_size <= 0.0f || export_scale <= 0.0f)
		{
			return false;
		}

		const float base_width = export_full_canvas ? ascii.cols * spacing_x : viewport_size.x;
		const float base_height = export_full_canvas ? ascii.rows * spacing_y : viewport_size.y;
		if (base_width <= 0.0f || base_height <= 0.0f)
			return false;

		const int export_width = std::max(1, static_cast<int>(std::ceil(base_width * export_scale)));
		const int export_height = std::max(1, static_cast<int>(std::ceil(base_height * export_scale)));
		const float scaled_spacing_x = spacing_x * export_scale;
		const float scaled_spacing_y = spacing_y * export_scale;
		const float scaled_glitch = glitch_intensity * export_scale;

		std::vector<unsigned char> pixels(
			static_cast<std::size_t>(export_width) * static_cast<std::size_t>(export_height) * 4);

		const unsigned char background_red = static_cast<unsigned char>(
			std::clamp(background_color.x * 255.0f, 0.0f, 255.0f));
		const unsigned char background_green = static_cast<unsigned char>(
			std::clamp(background_color.y * 255.0f, 0.0f, 255.0f));
		const unsigned char background_blue = static_cast<unsigned char>(
			std::clamp(background_color.z * 255.0f, 0.0f, 255.0f));
		const unsigned char background_alpha = static_cast<unsigned char>(
			std::clamp(background_color.w * 255.0f, 0.0f, 255.0f));

		for (int index = 0; index < export_width * export_height; ++index)
		{
			pixels[index * 4 + 0] = background_red;
			pixels[index * 4 + 1] = background_green;
			pixels[index * 4 + 2] = background_blue;
			pixels[index * 4 + 3] = background_alpha;
		}

		ImFontBaked *baked_font = ascii_font->GetFontBaked(ascii_font_size);
		if (!baked_font)
			return false;

		const char *preload_cursor = ascii.text.c_str();
		const char *text_end = preload_cursor + ascii.text.size();
		while (preload_cursor < text_end)
		{
			unsigned int codepoint = 0;
			const int consumed = ImTextCharFromUtf8(&codepoint, preload_cursor, text_end);
			if (consumed <= 0)
				break;
			preload_cursor += consumed;
			if (codepoint != '\n' && codepoint != '\r')
				baked_font->FindGlyph(static_cast<ImWchar>(codepoint));
		}

		unsigned char *font_pixels = nullptr;
		int atlas_width = 0;
		int atlas_height = 0;
		ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&font_pixels, &atlas_width, &atlas_height);
		if (!font_pixels || atlas_width <= 0 || atlas_height <= 0)
			return false;

		std::mt19937 random_engine(1337);
		std::uniform_real_distribution<float> jitter(-scaled_glitch, scaled_glitch);
		int column = 0;
		int row = 0;
		const char *cursor = ascii.text.c_str();

		while (cursor < text_end)
		{
			unsigned int codepoint = 0;
			const int consumed = ImTextCharFromUtf8(&codepoint, cursor, text_end);
			if (consumed <= 0)
				break;
			cursor += consumed;

			if (codepoint == '\r')
				continue;
			if (codepoint == '\n')
			{
				++row;
				column = 0;
				continue;
			}

			const ImFontGlyph *glyph = baked_font->FindGlyph(static_cast<ImWchar>(codepoint));
			if (glyph && glyph->Visible)
			{
				const float offset_x = scaled_glitch > 0.0f ? jitter(random_engine) : 0.0f;
				const float offset_y = scaled_glitch > 0.0f ? jitter(random_engine) : 0.0f;
				const float base_x = column * scaled_spacing_x + offset_x + glyph->X0 * export_scale;
				const float base_y = row * scaled_spacing_y + offset_y + glyph->Y0 * export_scale;

				const int source_u0 = static_cast<int>(glyph->U0 * atlas_width);
				const int source_v0 = static_cast<int>(glyph->V0 * atlas_height);
				const int source_u1 = static_cast<int>(glyph->U1 * atlas_width);
				const int source_v1 = static_cast<int>(glyph->V1 * atlas_height);
				const int glyph_width = std::max(1, source_u1 - source_u0);
				const int glyph_height = std::max(1, source_v1 - source_v0);
				const int draw_width = std::max(
					1, static_cast<int>(std::ceil((glyph->X1 - glyph->X0) * export_scale)));
				const int draw_height = std::max(
					1, static_cast<int>(std::ceil((glyph->Y1 - glyph->Y0) * export_scale)));

				for (int destination_y_offset = 0; destination_y_offset < draw_height; ++destination_y_offset)
				{
					for (int destination_x_offset = 0; destination_x_offset < draw_width; ++destination_x_offset)
					{
						const int destination_x = static_cast<int>(base_x) + destination_x_offset;
						const int destination_y = static_cast<int>(base_y) + destination_y_offset;
						if (destination_x < 0 || destination_x >= export_width ||
							destination_y < 0 || destination_y >= export_height)
						{
							continue;
						}

						const int source_x = source_u0 + destination_x_offset * glyph_width / draw_width;
						const int source_y = source_v0 + destination_y_offset * glyph_height / draw_height;
						if (source_x < 0 || source_x >= atlas_width || source_y < 0 || source_y >= atlas_height)
							continue;

						const int atlas_index = (source_y * atlas_width + source_x) * 4;
						const unsigned char glyph_alpha = font_pixels[atlas_index + 3];
						if (glyph_alpha == 0)
							continue;

						const int destination_index = (destination_y * export_width + destination_x) * 4;
						const float source_alpha = glyph_alpha / 255.0f * text_color.w;
						const float destination_alpha = pixels[destination_index + 3] / 255.0f;
						const float output_alpha = source_alpha + destination_alpha * (1.0f - source_alpha);
						if (output_alpha <= 0.0f)
							continue;

						const float destination_red = pixels[destination_index + 0] / 255.0f;
						const float destination_green = pixels[destination_index + 1] / 255.0f;
						const float destination_blue = pixels[destination_index + 2] / 255.0f;
						const float remaining_alpha = destination_alpha * (1.0f - source_alpha);
						const float output_red =
							(text_color.x * source_alpha + destination_red * remaining_alpha) / output_alpha;
						const float output_green =
							(text_color.y * source_alpha + destination_green * remaining_alpha) / output_alpha;
						const float output_blue =
							(text_color.z * source_alpha + destination_blue * remaining_alpha) / output_alpha;

						pixels[destination_index + 0] = static_cast<unsigned char>(
							std::clamp(output_red * 255.0f, 0.0f, 255.0f));
						pixels[destination_index + 1] = static_cast<unsigned char>(
							std::clamp(output_green * 255.0f, 0.0f, 255.0f));
						pixels[destination_index + 2] = static_cast<unsigned char>(
							std::clamp(output_blue * 255.0f, 0.0f, 255.0f));
						pixels[destination_index + 3] = static_cast<unsigned char>(
							std::clamp(output_alpha * 255.0f, 0.0f, 255.0f));
					}
				}
			}
			++column;
		}

		if (filepath.ends_with(".jpg") || filepath.ends_with(".jpeg"))
		{
			return stbi_write_jpg(
				filepath.c_str(), export_width, export_height, 4, pixels.data(), 95) != 0;
		}

		return stbi_write_png(
			filepath.c_str(), export_width, export_height, 4, pixels.data(), export_width * 4) != 0;
	}

	bool export_to_text()
	{
		return false;
	}
}
