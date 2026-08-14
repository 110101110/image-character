#pragma once

#include <string>
#include <vector>

namespace AsciiArt::ImageProcess
{
	enum class EdgeDetector
	{
		Outline,
		Sobel
	};

	enum class EdgeStyle
	{
		Simple,
		Directional
	};

	struct EdgeCharacters
	{
		std::string simple = "#";
		std::string horizontal = "─";
		std::string vertical = "│";
		std::string rising_diagonal = "╱";
		std::string falling_diagonal = "╲";
		bool operator==(const EdgeCharacters &) const = default;
	};

	struct Settings
	{
		float brightness = 0.0f;
		float contrast = 1.0f;
		float grayscale = 1.0f;
		bool invert = false;
		bool edge_detection = false;
		EdgeDetector edge_detector = EdgeDetector::Outline;
		EdgeStyle edge_style = EdgeStyle::Simple;
		float edge_threshold = 128.0f;
		EdgeCharacters edge_characters;
		bool operator==(const Settings &) const = default;
	};

	struct ProcessedImage
	{
		std::vector<unsigned char> pixels;
		int width = 0;
		int height = 0;
		int channels = 4;
	};

	float grayscale_value(float red, float green, float blue, float grayscale_mix);
	float invert_value(float value, bool invert);
	float contrast_value(float value, float contrast);
	float brightness_value(float value, float brightness);
	float process_pixel(float red, float green, float blue, const Settings &settings);
	bool outline_edge(float center, const float neighbors[8], float threshold);
	bool sobel_edge(const float samples[9], float threshold, float &gradient_x, float &gradient_y);
	ProcessedImage apply_to_image(const unsigned char *pixels, int width, int height,
		int channels, const Settings &settings);
}
