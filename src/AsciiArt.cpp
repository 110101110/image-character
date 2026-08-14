#include "AsciiArt.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

#include "imgui_internal.h"
#include "stb_image.h"

namespace AsciiArt
{
	AsciiOutput generate_ascii(const ImageBuffer &image, int target_columns, int target_rows,
		float brightness, float contrast, float grayscale,
		bool invert, const std::string &ramp, bool edge_detection,
		EdgeDetector edge_detector, EdgeStyle edge_style, float edge_threshold,
		const EdgeCharacters &edge_characters)
	{
		AsciiOutput output;
		if (!image.pixels || image.width <= 0 || image.height <= 0 || target_columns <= 0 || target_rows <= 0 || ramp.empty())
		{
			return output;
		}

		std::vector<std::string_view> ramp_characters;
		const char *ramp_cursor = ramp.data();
		const char *ramp_end = ramp_cursor + ramp.size();
		while (ramp_cursor < ramp_end)
		{
			unsigned int codepoint = 0;
			const int byte_count = ImTextCharFromUtf8(&codepoint, ramp_cursor, ramp_end);
			if (byte_count <= 0)
				break;
			ramp_characters.emplace_back(ramp_cursor, static_cast<std::size_t>(byte_count));
			ramp_cursor += byte_count;
		}

		if (ramp_characters.empty())
			return output;

		output.cols = target_columns;
		output.rows = target_rows;
		output.text.reserve((target_columns + 1) * target_rows);
		std::vector<float> luminance_grid(static_cast<std::size_t>(target_columns) * target_rows, 0.0f);

		const float cell_width = static_cast<float>(image.width) / target_columns;
		const float cell_height = static_cast<float>(image.height) / target_rows;
		const float grayscale_mix = std::clamp(grayscale, 0.0f, 1.0f);

		for (int row = 0; row < target_rows; ++row)
		{
			for (int column = 0; column < target_columns; ++column)
			{
				const int start_x = static_cast<int>(column * cell_width);
				const int start_y = static_cast<int>(row * cell_height);
				const int end_x = std::min(static_cast<int>((column + 1) * cell_width), image.width);
				const int end_y = std::min(static_cast<int>((row + 1) * cell_height), image.height);

				float total_luminance = 0.0f;
				int pixel_count = 0;

				for (int y = start_y; y < end_y; ++y)
				{
					for (int x = start_x; x < end_x; ++x)
					{
						const int index = (y * image.width + x) * image.channels;
						const float red = image.pixels[index + 0];
						const float green = image.pixels[index + 1];
						const float blue = image.pixels[index + 2];

					const float average_grayscale = (red + green + blue) / 3.0f;
					const float perceptual_luminance = 0.2126f * red + 0.7152f * green + 0.0722f * blue;
					float luminance = std::lerp(average_grayscale, perceptual_luminance, grayscale_mix);
						if (invert)
							luminance = 255.0f - luminance;
						total_luminance += luminance;
						++pixel_count;
					}
				}

				float average_luminance = pixel_count > 0
					? total_luminance / pixel_count
					: 0.0f;
				average_luminance = (average_luminance - 128.0f) * contrast + 128.0f + brightness;
				average_luminance = std::clamp(average_luminance, 0.0f, 255.0f);

				luminance_grid[static_cast<std::size_t>(row) * target_columns + column] =
					average_luminance;
			}
		}

		const auto sample = [&](int x, int y)
		{
			x = std::clamp(x, 0, target_columns - 1);
			y = std::clamp(y, 0, target_rows - 1);
			return luminance_grid[static_cast<std::size_t>(y) * target_columns + x];
		};

		const auto valid_character = [](const std::string &character, const char *fallback) -> std::string_view
		{
			if (character.empty())
				return fallback;
			unsigned int codepoint = 0;
			const int byte_count = ImTextCharFromUtf8(
				&codepoint, character.data(), character.data() + character.size());
			return byte_count > 0
				? std::string_view(character.data(), static_cast<std::size_t>(byte_count))
				: std::string_view(fallback);
		};
		const std::string_view simple_character = valid_character(edge_characters.simple, "#");
		const std::string_view horizontal_character = valid_character(edge_characters.horizontal, "─");
		const std::string_view vertical_character = valid_character(edge_characters.vertical, "│");
		const std::string_view rising_character = valid_character(edge_characters.rising_diagonal, "╱");
		const std::string_view falling_character = valid_character(edge_characters.falling_diagonal, "╲");

		const auto directional_character = [&](float gradient_x, float gradient_y) -> std::string_view
		{
			const float absolute_x = std::abs(gradient_x);
			const float absolute_y = std::abs(gradient_y);
			if (absolute_x > absolute_y * 2.0f)
				return vertical_character;
			if (absolute_y > absolute_x * 2.0f)
				return horizontal_character;
			return gradient_x * gradient_y >= 0.0f ? rising_character : falling_character;
		};

		const float threshold = std::clamp(edge_threshold, 0.0f, 255.0f);
		for (int row = 0; row < target_rows; ++row)
		{
			for (int column = 0; column < target_columns; ++column)
			{
				const float center = sample(column, row);
				bool is_edge = false;
				float gradient_x = 0.0f;
				float gradient_y = 0.0f;

				if (edge_detection && edge_detector == EdgeDetector::Outline)
				{
					const bool foreground = center >= threshold;
					for (int offset_y = -1; offset_y <= 1 && !is_edge; ++offset_y)
					{
						for (int offset_x = -1; offset_x <= 1; ++offset_x)
						{
							if ((offset_x != 0 || offset_y != 0) &&
								(sample(column + offset_x, row + offset_y) >= threshold) != foreground)
							{
								is_edge = true;
								break;
							}
						}
					}
					gradient_x = sample(column + 1, row) - sample(column - 1, row);
					gradient_y = sample(column, row + 1) - sample(column, row - 1);
				}
				else if (edge_detection)
				{
					gradient_x =
						-sample(column - 1, row - 1) + sample(column + 1, row - 1) -
						2.0f * sample(column - 1, row) + 2.0f * sample(column + 1, row) -
						sample(column - 1, row + 1) + sample(column + 1, row + 1);
					gradient_y =
						-sample(column - 1, row - 1) - 2.0f * sample(column, row - 1) -
						sample(column + 1, row - 1) + sample(column - 1, row + 1) +
						2.0f * sample(column, row + 1) + sample(column + 1, row + 1);
					const float magnitude = std::sqrt(
						gradient_x * gradient_x + gradient_y * gradient_y) / 4.0f;
					is_edge = magnitude >= threshold;
				}

				if (is_edge)
				{
					output.text += edge_style == EdgeStyle::Simple ? simple_character : directional_character(gradient_x, gradient_y);
				}
				else
				{
					const std::size_t ramp_index = static_cast<std::size_t>((center / 255.0f) * static_cast<float>(ramp_characters.size() - 1));
					output.text.append(ramp_characters[ramp_index]);
				}
			}
			output.text += '\n';
		}

		return output;
	}

	int calculate_locked_rows(const ImageBuffer &image, int columns, float spacing_x, float spacing_y)
	{
		if (image.width <= 0 || image.height <= 0 || columns <= 0 ||
			spacing_x <= 0.0f || spacing_y <= 0.0f)
		{
			return 1;
		}

		const float rows = columns * spacing_x * image.height /(spacing_y * static_cast<float>(image.width));
		return std::max(1, static_cast<int>(std::lround(rows)));
	}

	int calculate_locked_columns(const ImageBuffer &image, int rows, float spacing_x, float spacing_y)
	{
		if (image.width <= 0 || image.height <= 0 || rows <= 0 ||
			spacing_x <= 0.0f || spacing_y <= 0.0f)
		{
			return 1;
		}

		const float columns = rows * spacing_y * image.width / (spacing_x * static_cast<float>(image.height));
		return std::max(1, static_cast<int>(std::lround(columns)));
	}

	float calculate_locked_spacing_y(const ImageBuffer &image, int columns, int rows, float spacing_x)
	{
		if (image.width <= 0 || image.height <= 0 || columns <= 0 || rows <= 0 || spacing_x <= 0.0f)
			return spacing_x;

		return columns * spacing_x * image.height /
			(rows * static_cast<float>(image.width));
	}

	float calculate_locked_spacing_x(const ImageBuffer &image, int columns, int rows, float spacing_y)
	{
		if (image.width <= 0 || image.height <= 0 || columns <= 0 || rows <= 0 || spacing_y <= 0.0f)
			return spacing_y;

		return rows * spacing_y * image.width /
			(columns * static_cast<float>(image.height));
	}

	GLuint create_texture_from_pixels(const unsigned char *pixels, int width, int height, int channels)
	{
		if (!pixels || width <= 0 || height <= 0 || (channels != 3 && channels != 4))
			return 0;

		GLuint texture = 0;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);

		const GLenum format = channels == 4 ? GL_RGBA : GL_RGB;
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		return texture;
	}

	void destroy_image_buffer(ImageBuffer &image)
	{
		if (image.pixels)
			stbi_image_free(image.pixels);
		if (image.textureID)
			glDeleteTextures(1, &image.textureID);

		image = {};
	}
}
