#include "AsciiArt.h"

#include <algorithm>
#include <cmath>

#include "stb_image.h"

namespace AsciiArt
{
	AsciiOutput generate_ascii(const ImageBuffer &image, int target_columns, int target_rows,
		float brightness, float contrast, bool invert, const std::string &ramp)
	{
		AsciiOutput output;
		if (!image.pixels || image.width <= 0 || image.height <= 0 ||
			target_columns <= 0 || target_rows <= 0 || ramp.empty())
		{
			return output;
		}

		output.cols = target_columns;
		output.rows = target_rows;
		output.text.reserve((target_columns + 1) * target_rows);

		const float cell_width = static_cast<float>(image.width) / target_columns;
		const float cell_height = static_cast<float>(image.height) / target_rows;

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

						float luminance = 0.2126f * red + 0.7152f * green + 0.0722f * blue;
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

				const int ramp_index = static_cast<int>(
					(average_luminance / 255.0f) * static_cast<float>(ramp.size() - 1));
				output.text += ramp[ramp_index];
			}
			output.text += '\n';
		}

		return output;
	}

	int calculate_locked_rows(
		const ImageBuffer &image,
		int columns,
		float spacing_x,
		float spacing_y)
	{
		if (image.width <= 0 || image.height <= 0 || columns <= 0 ||
			spacing_x <= 0.0f || spacing_y <= 0.0f)
		{
			return 1;
		}

		const float rows =
			columns * spacing_x * image.height /
			(spacing_y * static_cast<float>(image.width));
		return std::max(1, static_cast<int>(std::lround(rows)));
	}

	int calculate_locked_columns(
		const ImageBuffer &image,
		int rows,
		float spacing_x,
		float spacing_y)
	{
		if (image.width <= 0 || image.height <= 0 || rows <= 0 ||
			spacing_x <= 0.0f || spacing_y <= 0.0f)
		{
			return 1;
		}

		const float columns =
			rows * spacing_y * image.width /
			(spacing_x * static_cast<float>(image.height));
		return std::max(1, static_cast<int>(std::lround(columns)));
	}

	float calculate_locked_spacing_y(
		const ImageBuffer &image,
		int columns,
		int rows,
		float spacing_x)
	{
		if (image.width <= 0 || image.height <= 0 || columns <= 0 || rows <= 0 || spacing_x <= 0.0f)
			return spacing_x;

		return columns * spacing_x * image.height /
			(rows * static_cast<float>(image.width));
	}

	float calculate_locked_spacing_x(
		const ImageBuffer &image,
		int columns,
		int rows,
		float spacing_y)
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
