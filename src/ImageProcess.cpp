#include "ImageProcess.h"

#include <algorithm>
#include <cmath>

namespace AsciiArt::ImageProcess
{
	float grayscale_value(float red, float green, float blue, float grayscale_mix)
	{
		const float average = (red + green + blue) / 3.0f;
		const float perceptual = 0.2126f * red + 0.7152f * green + 0.0722f * blue;
		return std::lerp(average, perceptual, std::clamp(grayscale_mix, 0.0f, 1.0f));
	}

	float invert_value(float value, bool invert) { return invert ? 255.0f - value : value; }
	float contrast_value(float value, float contrast) { return (value - 128.0f) * contrast + 128.0f; }
	float brightness_value(float value, float brightness) { return value + brightness; }

	float process_pixel(float red, float green, float blue, const Settings &settings)
	{
		const float grayscale = grayscale_value(red, green, blue, settings.grayscale);
		const float inverted = invert_value(grayscale, settings.invert);
		const float contrasted = contrast_value(inverted, settings.contrast);
		return std::clamp(brightness_value(contrasted, settings.brightness), 0.0f, 255.0f);
	}

	bool outline_edge(float center, const float neighbors[8], float threshold)
	{
		threshold = std::clamp(threshold, 0.0f, 255.0f);
		const bool foreground = center >= threshold;
		for (int index = 0; index < 8; ++index)
			if ((neighbors[index] >= threshold) != foreground)
				return true;
		return false;
	}

	bool sobel_edge(const float samples[9], float threshold, float &gradient_x, float &gradient_y)
	{
		gradient_x = -samples[0] + samples[2] - 2.0f * samples[3] + 2.0f * samples[5] - samples[6] + samples[8];
		gradient_y = -samples[0] - 2.0f * samples[1] - samples[2] + samples[6] + 2.0f * samples[7] + samples[8];
		return std::sqrt(gradient_x * gradient_x + gradient_y * gradient_y) / 4.0f >=
			std::clamp(threshold, 0.0f, 255.0f);
	}

	ProcessedImage apply_to_image(const unsigned char *pixels, int width, int height,
		int channels, const Settings &settings)
	{
		ProcessedImage output;
		if (!pixels || width <= 0 || height <= 0 || channels < 3)
			return output;

		output.width = width;
		output.height = height;
		output.pixels.resize(static_cast<std::size_t>(width) * height * 4);
		std::vector<float> luminance(static_cast<std::size_t>(width) * height);

		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				const std::size_t source = (static_cast<std::size_t>(y) * width + x) * channels;
				luminance[static_cast<std::size_t>(y) * width + x] = process_pixel(
					pixels[source], pixels[source + 1], pixels[source + 2], settings);
			}
		}

		const auto sample = [&](int x, int y)
		{
			x = std::clamp(x, 0, width - 1);
			y = std::clamp(y, 0, height - 1);
			return luminance[static_cast<std::size_t>(y) * width + x];
		};

		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				float value = sample(x, y);
				bool is_edge = false;
				if (settings.edge_detection && settings.edge_detector == EdgeDetector::Outline)
				{
					float neighbors[8];
					int index = 0;
					for (int offset_y = -1; offset_y <= 1; ++offset_y)
						for (int offset_x = -1; offset_x <= 1; ++offset_x)
							if (offset_x != 0 || offset_y != 0)
								neighbors[index++] = sample(x + offset_x, y + offset_y);
					is_edge = outline_edge(value, neighbors, settings.edge_threshold);
				}
				else if (settings.edge_detection)
				{
					float samples[9];
					int index = 0;
					for (int offset_y = -1; offset_y <= 1; ++offset_y)
						for (int offset_x = -1; offset_x <= 1; ++offset_x)
							samples[index++] = sample(x + offset_x, y + offset_y);
					float gradient_x = 0.0f;
					float gradient_y = 0.0f;
					is_edge = sobel_edge(
						samples, settings.edge_threshold, gradient_x, gradient_y);
				}

				if (is_edge)
					value = 0.0f;
				const unsigned char gray = static_cast<unsigned char>(std::lround(value));
				const std::size_t destination = (static_cast<std::size_t>(y) * width + x) * 4;
				output.pixels[destination] = gray;
				output.pixels[destination + 1] = gray;
				output.pixels[destination + 2] = gray;
				output.pixels[destination + 3] = channels >= 4
					? pixels[(static_cast<std::size_t>(y) * width + x) * channels + 3]
					: 255;
			}
		}
		return output;
	}
}
