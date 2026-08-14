#pragma once

#include <array>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#include "AsciiArt.h"
#include "RampEditor.h"
#include "TextEditor.h"
#include "imgui.h"

namespace AsciiArt::AppState
{
	struct ImageState
	{
		ImageBuffer current;
		GLuint processed_texture = 0;
		std::string file_path;
		AsciiOutput ascii;
		Text::Layout generated_layout;
	};

	struct RampState
	{
		RampEditor::State editor;
		std::array<char, 16> simple_edge{'#', '\0'};
		std::array<char, 16> horizontal_edge{};
		std::array<char, 16> vertical_edge{};
		std::array<char, 16> rising_edge{};
		std::array<char, 16> falling_edge{};
	};

	struct FontState
	{
		int selected = 0;
		bool pending_rebuild = false;
		FontSet fonts;
		bool lock_aspect = true;
	};

	struct GlitchState
	{
		bool animate = false;
		std::mt19937 random{1337};
	};

	struct ViewportState
	{
		TextEditor::State editor;
		float preferred_editor_height = 300.0f;
		bool control_panel_collapsed = false;
	};

	struct ExportState
	{
		bool pending_image = false;
		std::string image_path;
		bool pending_text = false;
		std::string text_path;
	};

	struct GifState
	{
		std::filesystem::path temporary_directory;
		int frame_count = 0;
		int delay = 10;
		int export_area = 0;
		bool pending_frame = false;
		bool pending_generation = false;
		std::string output_path;
		std::string status;
	};

	struct State
	{
		ImageState image;
		RampState ramp;
		FontState font;
		AsciiGenerationOptions generation;
		ImageExportOptions image_export;
		ViewportTransform viewport_transform;
		float base_opacity = 0.4f;
		bool show_base_layer = true;
		bool show_processed_base = false;
		bool show_ascii_layer = true;
		GlitchState glitch;
		ViewportState viewport;
		ExportState export_job;
		GifState gif;
	};
}
