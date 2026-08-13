#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <vector>

#include <glad/glad.h>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include "stb_image.h"

#include "NativeFileDialog.h"
#include "AsciiArt.h"

using namespace AsciiArt;
namespace fs = std::filesystem;

struct RampEditorState
{
	std::size_t cursor_byte = 0;
};

struct AsciiEditorState
{
	bool enabled = false;
	bool modified = false;
	bool soft_wrap = true;
	float width = 800.0f;
	std::vector<char> buffer = std::vector<char>(1024 * 1024, '\0');
};

static void set_editor_text(AsciiEditorState &editor, const std::string &text)
{
	const std::size_t copy_size = std::min(text.size(), editor.buffer.size() - 1);
	std::memcpy(editor.buffer.data(), text.data(), copy_size);
	editor.buffer[copy_size] = '\0';
}

static AsciiOutput layout_edited_ascii(const char *text, bool soft_wrap, int wrap_columns)
{
	AsciiOutput output;
	if (!text || *text == '\0')
		return output;

	wrap_columns = std::max(1, wrap_columns);
	const char *cursor = text;
	const char *end = text + std::strlen(text);
	int column = 0;
	int maximum_columns = 0;
	int rows = 1;

	while (cursor < end)
	{
		unsigned int codepoint = 0;
		const int byte_count = ImTextCharFromUtf8(&codepoint, cursor, end);
		if (byte_count <= 0)
			break;

		if (codepoint == '\n')
		{
			output.text.push_back('\n');
			maximum_columns = std::max(maximum_columns, column);
			column = 0;
			if (cursor + byte_count < end)
				++rows;
			cursor += byte_count;
			continue;
		}

		if (soft_wrap && column >= wrap_columns)
		{
			output.text.push_back('\n');
			maximum_columns = std::max(maximum_columns, column);
			column = 0;
			++rows;
		}

		output.text.append(cursor, static_cast<std::size_t>(byte_count));
		cursor += byte_count;
		++column;
	}

	output.cols = std::max(maximum_columns, column);
	output.rows = rows;
	return output;
}

static int longest_utf8_line(const char *text)
{
	if (!text)
		return 0;

	const char *cursor = text;
	const char *end = text + std::strlen(text);
	int current_columns = 0;
	int maximum_columns = 0;
	while (cursor < end)
	{
		unsigned int codepoint = 0;
		const int byte_count = ImTextCharFromUtf8(&codepoint, cursor, end);
		if (byte_count <= 0)
			break;
		cursor += byte_count;
		if (codepoint == '\n')
		{
			maximum_columns = std::max(maximum_columns, current_columns);
			current_columns = 0;
		}
		else if (codepoint != '\r')
		{
			++current_columns;
		}
	}
	return std::max(maximum_columns, current_columns);
}

static fs::path create_gif_temp_directory()
{
	std::random_device random_device;
	for (int attempt = 0; attempt < 32; ++attempt)
	{
		const fs::path candidate = fs::temp_directory_path() /
			("ascii_signature_gif_" + std::to_string(random_device()));
		std::error_code error;
		if (fs::create_directory(candidate, error))
			return candidate;
	}
	return {};
}

static std::string shell_quote(const fs::path &path)
{
	std::string quoted = "'";
	for (char character : path.string())
	{
		if (character == '\'')
			quoted += "'\\''";
		else
			quoted += character;
	}
	quoted += '\'';
	return quoted;
}

static int capture_ramp_cursor(ImGuiInputTextCallbackData *data)
{
	auto *state = static_cast<RampEditorState *>(data->UserData);
	state->cursor_byte = static_cast<std::size_t>(std::max(0, data->CursorPos));
	return 0;
}

static bool insert_utf8_at_cursor(char *buffer, std::size_t capacity, std::size_t &cursor_byte, const char *character, std::size_t character_size)
{
	const std::size_t current_size = std::strlen(buffer);
	cursor_byte = std::min(cursor_byte, current_size);
	if (current_size + character_size >= capacity)
		return false;

	std::memmove(
		buffer + cursor_byte + character_size,
		buffer + cursor_byte,
		current_size - cursor_byte + 1);
	std::memcpy(buffer + cursor_byte, character, character_size);
	cursor_byte += character_size;
	return true;
}

static void present_imgui_frame(GLFWwindow *window)
{
	ImDrawData *draw_data = ImGui::GetDrawData();
	if (!draw_data || draw_data->CmdListsCount == 0)
		return;

	int window_width = 0;
	int window_height = 0;
	int framebuffer_width = 0;
	int framebuffer_height = 0;
	glfwGetWindowSize(window, &window_width, &window_height);
	glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
	if (window_width <= 0 || window_height <= 0 || framebuffer_width <= 0 || framebuffer_height <= 0)
		return;

	// During macOS live resize, GLFW may remain inside its native event loop. Update
	// the cached draw data to the current drawable size so Cocoa does not stretch
	// the previous framebuffer (which would visually resize every UI element).
	const ImVec2 previous_display_size = draw_data->DisplaySize;
	const ImVec2 previous_framebuffer_scale = draw_data->FramebufferScale;
	draw_data->DisplaySize = ImVec2(
		static_cast<float>(window_width),
		static_cast<float>(window_height));
	draw_data->FramebufferScale = ImVec2(
		static_cast<float>(framebuffer_width) / static_cast<float>(window_width),
		static_cast<float>(framebuffer_height) / static_cast<float>(window_height));

	glfwMakeContextCurrent(window);
	glViewport(0, 0, framebuffer_width, framebuffer_height);
	glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(draw_data);
	glfwSwapBuffers(window);

	draw_data->DisplaySize = previous_display_size;
	draw_data->FramebufferScale = previous_framebuffer_scale;
}

static void refresh_window_during_live_resize(GLFWwindow *window)
{
	present_imgui_frame(window);
}

int main(int argc, char **argv)
{
	const fs::path executable_path = argc > 0
		? fs::absolute(argv[0])
		: fs::current_path() / "image_character";
	const fs::path gif_script_path = executable_path.parent_path() / "make_gif.sh";
	if (!glfwInit())
		return -1;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow *window = glfwCreateWindow(Config::window_width, Config::window_height, Config::window_title, nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}
	glfwSetWindowSizeLimits(window, 800, 600, GLFW_DONT_CARE, GLFW_DONT_CARE);
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigWindowsResizeFromEdges = true;
	ImGui::StyleColorsLight();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(Config::glsl_version);
	glfwSetWindowRefreshCallback(window, refresh_window_during_live_resize);

	ImageBuffer current_img;
	std::string current_file_path = "";
	AsciiOutput ascii_art;

	std::array<char, 1024> ramp_buffer{};
	std::snprintf(ramp_buffer.data(), ramp_buffer.size(), "%s", Config::default_ramp);
	RampEditorState ramp_editor{std::strlen(ramp_buffer.data())};

	int selected_ascii_font = 0;
	float ascii_font_size = Config::default_ascii_font_size;
	bool pending_font_rebuild = false;
	FontSet fonts = rebuild_font_atlas(builtin_ascii_fonts[selected_ascii_font], ascii_font_size);
	float char_spacing_x = Config::default_spacing_x;
	float char_spacing_y = Config::default_spacing_y;

	int target_columns = Config::default_columns;
	int target_rows = Config::default_rows;
	bool lock_aspect = true;

	// Tuning
	float brightness = 0.0f;
	float contrast = 1.0f;
	float grayscale = 1.0f;
	bool invert_image = false;
	bool edge_detection = false;
	EdgeDetector edge_detector = EdgeDetector::Outline;
	EdgeStyle edge_style = EdgeStyle::Simple;
	float edge_threshold = 128.0f;
	EdgeCharacters edge_characters;
	std::array<char, 16> simple_edge_buffer{'#', '\0'};
	std::array<char, 16> horizontal_edge_buffer{};
	std::array<char, 16> vertical_edge_buffer{};
	std::array<char, 16> rising_edge_buffer{};
	std::array<char, 16> falling_edge_buffer{};
	std::snprintf(horizontal_edge_buffer.data(), horizontal_edge_buffer.size(), "%s", "─");
	std::snprintf(vertical_edge_buffer.data(), vertical_edge_buffer.size(), "%s", "│");
	std::snprintf(rising_edge_buffer.data(), rising_edge_buffer.size(), "%s", "╱");
	std::snprintf(falling_edge_buffer.data(), falling_edge_buffer.size(), "%s", "╲");
	float base_opacity = 0.4f;
	bool show_base_layer = true;
	bool show_ascii_layer = true;

	ImVec4 text_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	ImVec4 bg_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

	// Glitch
	float glitch_intensity = 0.0f;
	bool glitch_per_frame = false;

	bool export_full_canvas = false;

	ImVec2 current_viewport_size = ImVec2(0, 0); // Stores live Viewport dimensions
	ImVec2 current_viewport_scroll = ImVec2(0, 0);
	float viewport_zoom = 1.0f;
	AsciiEditorState text_editor;
	float preferred_editor_height = 300.0f;
	bool control_panel_collapsed = false;

	bool pending_image_export = false;
	std::string image_export_path;
	bool pending_text_export = false;
	std::string text_export_path;

	fs::path gif_temp_directory;
	int gif_frame_count = 0;
	int gif_delay = 10;
	int gif_export_area = 0; // 0 = viewport, 1 = full canvas
	bool pending_gif_frame = false;
	bool pending_gif_generation = false;
	std::string gif_output_path;
	std::string gif_status;

	std::mt19937 main_rng(1337);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS && glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GLFW_TRUE);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		const float control_panel_width = control_panel_collapsed ? 36.0f : Config::sidebar_width;
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(control_panel_width, io.DisplaySize.y), ImGuiCond_Always);
		ImGui::Begin("Matrix Controls", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

		if (control_panel_collapsed)
		{
			if (ImGui::Button(">", ImVec2(-1.0f, 30.0f)))
				control_panel_collapsed = false;
		}
		else
		{
			ImGui::TextUnformatted("Matrix Controls");
			ImGui::SameLine();
			if (ImGui::Button("<<"))
				control_panel_collapsed = true;
			ImGui::Separator();

		ImGui::Text("1. Reference Image");
		if (ImGui::Button("Open File ...", ImVec2(-1, 30)))
		{
			std::string selected_path = OpenNativeImageDialog();
			if (!selected_path.empty())
			{
				current_file_path = selected_path;
				int w, h, ch;
				unsigned char *data = stbi_load(current_file_path.c_str(), &w, &h, &ch, 4);
				//if fails to load data, convert image file to the good format with magick (with a bash script)
				if (data)
				{
					destroy_image_buffer(current_img);

					current_img.pixels = data;
					current_img.width = w;
					current_img.height = h;
					current_img.channels = 4;
					current_img.textureID = create_texture_from_pixels(data, w, h, 4);

					target_columns = 80;
					target_rows = calculate_locked_rows(current_img, target_columns, char_spacing_x, char_spacing_y);

					ascii_art = generate_ascii(
						current_img, target_columns, target_rows, brightness, contrast, grayscale,
						invert_image, ramp_buffer.data(), edge_detection, edge_detector,
						edge_style, edge_threshold, edge_characters);
				}
			}
		}

		ImGui::Separator();
		ImGui::Text("2. Image Grid & Ratio Controls");
		const bool aspect_lock_changed = ImGui::Checkbox("Lock Aspect Ratio", &lock_aspect);
		if (aspect_lock_changed && lock_aspect && current_img.pixels)
			char_spacing_y = calculate_locked_spacing_y(current_img, target_columns, target_rows, char_spacing_x);
		ImGui::Text("Grid Resolution: %d x %d", ascii_art.cols, ascii_art.rows);

		bool grid_changed = false;
		if (ImGui::DragInt("Columns (X)", &target_columns, 1, 10, 500))
		{
			grid_changed = true;
			if (lock_aspect && current_img.height > 0 && current_img.width > 0)
				target_rows = calculate_locked_rows(current_img, target_columns, char_spacing_x, char_spacing_y);
		}
		if (ImGui::DragInt("Rows (Y)", &target_rows, 1, 10, 500))
		{
			grid_changed = true;
			if (lock_aspect && current_img.height > 0 && current_img.width > 0)
				target_columns = calculate_locked_columns(current_img, target_rows, char_spacing_x, char_spacing_y);
		}
		if (grid_changed && current_img.pixels)
			ascii_art = generate_ascii(
				current_img, target_columns, target_rows, brightness, contrast, grayscale,
				invert_image, ramp_buffer.data(), edge_detection, edge_detector,
				edge_style, edge_threshold, edge_characters);

		ImGui::Separator();
		ImGui::Text("3. Image Processing");
		bool processing_changed = false;
		processing_changed |= ImGui::Checkbox("Invert Image Colors", &invert_image);
		processing_changed |= ImGui::SliderFloat("Brightness", &brightness, -100.0f, 100.0f);
		processing_changed |= ImGui::SliderFloat("Contrast", &contrast, 0.1f, 3.0f);
			processing_changed |= ImGui::SliderFloat("Grayscale", &grayscale, 0.0f, 1.0f, "%.2f");
			if (ImGui::Button(
				edge_detection ? "Disable Edge Detection" : "Enable Edge Detection",
				ImVec2(-1.0f, 0.0f)))
			{
				edge_detection = !edge_detection;
				processing_changed = true;
			}
			if (edge_detection)
			{
				int detector_index = edge_detector == EdgeDetector::Outline ? 0 : 1;
				const char *detectors[] = {"Threshold Outline", "Sobel Gradient"};
				if (ImGui::Combo("Edge Detector", &detector_index, detectors, IM_ARRAYSIZE(detectors)))
				{
					edge_detector = detector_index == 0 ? EdgeDetector::Outline : EdgeDetector::Sobel;
					processing_changed = true;
				}

				int style_index = edge_style == EdgeStyle::Simple ? 0 : 1;
				const char *styles[] = {"Simple (#)", "Directional"};
				if (ImGui::Combo("Edge Style", &style_index, styles, IM_ARRAYSIZE(styles)))
				{
					edge_style = style_index == 0 ? EdgeStyle::Simple : EdgeStyle::Directional;
					processing_changed = true;
				}
				if (edge_style == EdgeStyle::Simple)
				{
					if (ImGui::InputText(
						"Simple Edge Character", simple_edge_buffer.data(), simple_edge_buffer.size()))
					{
						edge_characters.simple = simple_edge_buffer.data();
						processing_changed = true;
					}
				}
				else
				{
					bool characters_changed = false;
					characters_changed |= ImGui::InputText(
						"Horizontal Edge", horizontal_edge_buffer.data(), horizontal_edge_buffer.size());
					characters_changed |= ImGui::InputText(
						"Vertical Edge", vertical_edge_buffer.data(), vertical_edge_buffer.size());
					characters_changed |= ImGui::InputText(
						"Rising Diagonal", rising_edge_buffer.data(), rising_edge_buffer.size());
					characters_changed |= ImGui::InputText(
						"Falling Diagonal", falling_edge_buffer.data(), falling_edge_buffer.size());
					if (characters_changed)
					{
						edge_characters.horizontal = horizontal_edge_buffer.data();
						edge_characters.vertical = vertical_edge_buffer.data();
						edge_characters.rising_diagonal = rising_edge_buffer.data();
						edge_characters.falling_diagonal = falling_edge_buffer.data();
						processing_changed = true;
					}
				}
				processing_changed |= ImGui::SliderFloat(
					"Edge Threshold", &edge_threshold, 0.0f, 255.0f, "%.0f");
			}

		ImGui::Separator();
		ImGui::Text("4. Ramp Characters");
		if (ImGui::Button("Default Ramp"))
		{
			std::snprintf(ramp_buffer.data(), ramp_buffer.size(), "%s", Config::default_ramp);
			ramp_editor.cursor_byte = std::strlen(ramp_buffer.data());
			processing_changed = true;
		}
		if (ImGui::InputText("Ramp", ramp_buffer.data(), ramp_buffer.size(),
			ImGuiInputTextFlags_CallbackAlways, capture_ramp_cursor, &ramp_editor))
		{
			processing_changed = true;
		}

		if (ImGui::TreeNodeEx("Special Symbol Keyboard", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
				ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			for (int group_index = 0;
				 group_index < static_cast<int>(special_symbol_group_count);
				 ++group_index)
			{
				const SymbolGroup &group = special_symbol_groups[group_index];
				ImGui::TextUnformatted(group.name);
				ImGui::PushID(group_index);

				const char *symbol_cursor = group.symbols;
				const char *symbol_end = symbol_cursor + std::strlen(group.symbols);
				int symbol_index = 0;
				while (symbol_cursor < symbol_end)
				{
					unsigned int codepoint = 0;
					const int byte_count = ImTextCharFromUtf8(&codepoint, symbol_cursor, symbol_end);
					if (byte_count <= 0)
						break;

					const std::string button_label(symbol_cursor, static_cast<std::size_t>(byte_count));
					ImGui::PushID(symbol_index);
					if (ImGui::Button(button_label.c_str(), ImVec2(32.0f, 0.0f)))
					{
						if (insert_utf8_at_cursor(
							ramp_buffer.data(),
							ramp_buffer.size(),
							ramp_editor.cursor_byte,
							symbol_cursor,
							static_cast<std::size_t>(byte_count)))
						{
							processing_changed = true;
						}
					}
					ImGui::PopID();

					symbol_cursor += byte_count;
					++symbol_index;
					if (symbol_cursor < symbol_end && symbol_index % 6 != 0)
						ImGui::SameLine();
				}
				ImGui::PopID();
			}
			ImGui::TreePop();
		}

		if (processing_changed)
		{
			if (current_img.pixels)
			{
					ascii_art = generate_ascii(
						current_img, target_columns, target_rows, brightness, contrast, grayscale,
						invert_image, ramp_buffer.data(), edge_detection, edge_detector,
						edge_style, edge_threshold, edge_characters);
			}
		}

		ImGui::Separator();
		ImGui::Text("5. Character Setting");
		if (ImGui::BeginCombo("Font", builtin_ascii_fonts[selected_ascii_font].name))
		{
			for (int i = 0; i < static_cast<int>(builtin_ascii_font_count); ++i)
			{
				bool selected = (selected_ascii_font == i);

				if (ImGui::Selectable(builtin_ascii_fonts[i].name, selected))
				{
					selected_ascii_font = i;
					pending_font_rebuild = true;
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		ImGui::DragFloat("Font Size", &ascii_font_size,0.1f, Config::minimum_ascii_font_size,Config::maximum_ascii_font_size, "%.1f");
		if (ImGui::IsItemDeactivatedAfterEdit())
			pending_font_rebuild = true;
		const bool spacing_x_changed = ImGui::DragFloat(
			"Horizontal Spacing", &char_spacing_x, 0.1f, 0.1f, 100.0f, "%.1f");
		const bool spacing_y_changed = ImGui::DragFloat(
			"Vertical Spacing", &char_spacing_y, 0.1f, 0.1f, 100.0f, "%.1f");

		if (lock_aspect && current_img.pixels)
		{
			if (spacing_x_changed)
				char_spacing_y = calculate_locked_spacing_y(current_img, target_columns, target_rows, char_spacing_x);
			else if (spacing_y_changed)
				char_spacing_x = calculate_locked_spacing_x(current_img, target_columns, target_rows, char_spacing_y);
		}

		ImGui::Separator();
		ImGui::Text("6. Glitch Mechanics & Special Effects");
		ImGui::SliderFloat("Glitch Jitter", &glitch_intensity, 0.0f, 50.0f);
		ImGui::Checkbox("Animate Glitch", &glitch_per_frame);

		ImGui::Separator();
		ImGui::Text("7. Color & Layer Opacity");
		ImGui::ColorEdit4("Canvas Background", (float *)&bg_color);
		ImGui::ColorEdit4("Text Color", (float *)&text_color);
		ImGui::SliderFloat("Base Layer Opacity", &base_opacity, 0.0f, 1.0f);
		ImGui::Checkbox("Show Base Image", &show_base_layer);
		ImGui::Checkbox("Show ASCII Overlay", &show_ascii_layer);

		ImGui::Separator();
		ImGui::Text("8. Exports");
		ImGui::Checkbox("Export Entire ASCII Canvas", &export_full_canvas);
		if (ImGui::Button("Export Image (.PNG / .JPG)", ImVec2(-1, 30)))
		{
			std::string save_path = SaveNativeFileDialog("ascii_art.png");
			if (!save_path.empty())
			{
				image_export_path = save_path;
				pending_image_export = true;
			}
		}
			if (ImGui::Button("Export Text (.TXT)", ImVec2(-1, 30)))
		{
			std::string save_path = SaveNativeFileDialog("ascii_art.txt");
			if (!save_path.empty())
			{
				text_export_path = save_path;
				pending_text_export = true;
				}
			}

			ImGui::Separator();
			ImGui::Text("9. Animated GIF (%d / 16 frames)", gif_frame_count);
			const char *gif_areas[] = {"Current Viewport", "Full ASCII Canvas"};
			ImGui::Combo("GIF Frame Area", &gif_export_area, gif_areas, IM_ARRAYSIZE(gif_areas));
			ImGui::SliderInt("GIF Delay (1/100 sec)", &gif_delay, 1, 100);

			ImGui::BeginDisabled(
				gif_frame_count >= 16 || ascii_art.text.empty() || pending_gif_frame);
			if (ImGui::Button("Add GIF Frame", ImVec2(-1, 30)))
			{
				if (gif_temp_directory.empty())
					gif_temp_directory = create_gif_temp_directory();
				if (gif_temp_directory.empty())
					gif_status = "Failed to create temporary frame directory.";
				else
					pending_gif_frame = true;
			}
			ImGui::EndDisabled();

			ImGui::BeginDisabled(gif_frame_count == 0 || pending_gif_generation);
			if (ImGui::Button("Generate GIF", ImVec2(-1, 30)))
			{
				std::string save_path = SaveNativeFileDialog("ascii_animation.gif");
				if (!save_path.empty())
				{
					if (!save_path.ends_with(".gif"))
						save_path += ".gif";
					gif_output_path = save_path;
					pending_gif_generation = true;
				}
			}
			ImGui::EndDisabled();

			if (gif_frame_count > 0 && ImGui::Button("Clear GIF Frames", ImVec2(-1, 0)))
			{
				std::error_code error;
				fs::remove_all(gif_temp_directory, error);
				gif_temp_directory.clear();
				gif_frame_count = 0;
				gif_status = error ? "Failed to remove some temporary frames." : "GIF frames cleared.";
			}
			if (!gif_status.empty())
				ImGui::TextWrapped("%s", gif_status.c_str());
			}
		ImGui::End();

		// Viewport tools
		const float viewport_x = control_panel_width;
		const float viewport_width = std::max(1.0f, io.DisplaySize.x - viewport_x);
		constexpr float tools_height = 72.0f;
		ImGui::SetNextWindowPos(ImVec2(viewport_x, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(
			ImVec2(viewport_width, tools_height),
			ImGuiCond_Always);
		ImGui::Begin("Viewport Tools",nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

		ImGui::SetNextItemWidth(140.0f);
		ImGui::SliderFloat("Zoom", &viewport_zoom, 0.1f, 8.0f, "%.2fx", ImGuiSliderFlags_Logarithmic);
		viewport_zoom = std::clamp(viewport_zoom, 0.1f, 8.0f);
		ImGui::SameLine();
		if (ImGui::Button("100%"))
			viewport_zoom = 1.0f;
		ImGui::SameLine();
		const bool was_editing = text_editor.enabled;
		ImGui::Checkbox("Text Editor", &text_editor.enabled);
		if (text_editor.enabled && !was_editing && !text_editor.modified)
			set_editor_text(text_editor, ascii_art.text);
		ImGui::SameLine();
		ImGui::Checkbox("Soft Wrap", &text_editor.soft_wrap);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.0f);
		ImGui::DragFloat("Editor Width", &text_editor.width, 2.0f, 120.0f, 8000.0f, "%.0f px");
		text_editor.width = std::clamp(text_editor.width, 120.0f, 8000.0f);
		ImGui::End();

		if (text_editor.enabled && !text_editor.modified && std::strcmp(text_editor.buffer.data(), ascii_art.text.c_str()) != 0)
			set_editor_text(text_editor, ascii_art.text);

		// ASCII editor
		ImGuiWindow *existing_canvas_window = ImGui::FindWindowByName("Canvas Viewport");
		const bool canvas_was_collapsed = existing_canvas_window && existing_canvas_window->Collapsed;
		const float collapsed_bar_height = ImGui::GetFrameHeight();
		const float available_stack_height = std::max(1.0f, io.DisplaySize.y - tools_height);
		const float maximum_editor_height = std::max(collapsed_bar_height, available_stack_height - collapsed_bar_height);
		preferred_editor_height = std::clamp(preferred_editor_height, collapsed_bar_height, maximum_editor_height);
		float requested_editor_height = text_editor.enabled ? preferred_editor_height : collapsed_bar_height;
		if (text_editor.enabled && canvas_was_collapsed)
			requested_editor_height = std::max(collapsed_bar_height, available_stack_height - collapsed_bar_height);

		ImGui::SetNextWindowPos(ImVec2(viewport_x, tools_height), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(viewport_width, requested_editor_height), ImGuiCond_Always);
		ImGui::SetNextWindowSizeConstraints(ImVec2(viewport_width, collapsed_bar_height), ImVec2(viewport_width, maximum_editor_height));

		const bool editor_expanded = ImGui::Begin("ASCII Editor", nullptr,
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_HorizontalScrollbar);
		const float editor_layout_height = ImGui::GetWindowHeight();

		if (editor_expanded && text_editor.enabled && !canvas_was_collapsed)
			preferred_editor_height = editor_layout_height;

		if (editor_expanded && text_editor.enabled)
		{
			if (ImGui::Button("Reset from Generated"))
			{
				set_editor_text(text_editor, ascii_art.text);
				text_editor.modified = false;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("Edits are preserved until Reset from Generated is pressed.");

			const float editor_content_width = std::max(text_editor.width, longest_utf8_line(text_editor.buffer.data()) * char_spacing_x * viewport_zoom);
			const float editor_footer_height = ImGui::GetTextLineHeightWithSpacing();
			const float input_height = std::max(1.0f, ImGui::GetContentRegionAvail().y - editor_footer_height);
			ImGui::PushFont(fonts.ascii_font, ascii_font_size * viewport_zoom);

			const bool editor_changed = ImGui::InputTextMultiline("##AsciiTextEditor", text_editor.buffer.data(), text_editor.buffer.size(),
				ImVec2(std::max(120.0f, editor_content_width), input_height), ImGuiInputTextFlags_AllowTabInput);

			ImGui::PopFont();
			if (editor_changed)
				text_editor.modified = true;
		}
		ImGui::End();

		const int editor_wrap_columns = std::max(1, static_cast<int>(text_editor.width / std::max(0.1f, char_spacing_x * viewport_zoom)));
		const AsciiOutput displayed_ascii = text_editor.enabled ? layout_edited_ascii(text_editor.buffer.data(), text_editor.soft_wrap, editor_wrap_columns) : ascii_art;

		// Canvas viewport
		const float canvas_y = canvas_was_collapsed ? std::max(tools_height, io.DisplaySize.y - collapsed_bar_height) : tools_height + editor_layout_height;
		ImGui::SetNextWindowPos(ImVec2(viewport_x, canvas_y), ImGuiCond_Always);
		ImGui::SetNextWindowSize(
			ImVec2(viewport_width, std::max(1.0f, io.DisplaySize.y - canvas_y)),
			ImGuiCond_Always);
		const bool canvas_expanded = ImGui::Begin("Canvas Viewport", nullptr,
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_HorizontalScrollbar);

		if (canvas_expanded)
		{
			const bool viewport_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
			if (viewport_hovered && (io.KeyCtrl || io.KeySuper) && io.MouseWheel != 0.0f)
				viewport_zoom = std::clamp(viewport_zoom * (io.MouseWheel > 0.0f ? 1.1f : 1.0f / 1.1f), 0.1f, 8.0f);

		current_viewport_size = ImGui::GetContentRegionAvail();
		current_viewport_scroll = ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
		ImDrawList *draw_list = ImGui::GetWindowDrawList();
		ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

		if (current_img.textureID > 0)
		{
			const float unscaled_text_width = displayed_ascii.cols * char_spacing_x;
			const float unscaled_text_height = displayed_ascii.rows * char_spacing_y;
			const float unscaled_base_width = ascii_art.cols * char_spacing_x;
			const float unscaled_base_height = ascii_art.rows * char_spacing_y;
			const float canvas_scale = viewport_zoom;
			const float canvas_w = std::max(unscaled_text_width, unscaled_base_width) * canvas_scale;
			const float canvas_h = std::max(unscaled_text_height, unscaled_base_height) * canvas_scale;

			if (show_base_layer)
			{
				ImU32 tint = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, base_opacity));
					draw_list->AddImage((void *)(intptr_t)current_img.textureID, canvas_pos,
					ImVec2(canvas_pos.x + unscaled_base_width * canvas_scale, canvas_pos.y + unscaled_base_height * canvas_scale),
					ImVec2(0, 0), ImVec2(1, 1),tint);
			}

			if (show_ascii_layer && !displayed_ascii.text.empty())
			{
				ImU32 txt_col = ImGui::ColorConvertFloat4ToU32(text_color);

				if (!glitch_per_frame)
					main_rng.seed(1337);
				std::uniform_real_distribution<float> jitter_dist(-glitch_intensity, glitch_intensity);

				int col = 0;
				int row = 0;

				const char *text_ptr = displayed_ascii.text.c_str();
				const char *text_end = text_ptr + displayed_ascii.text.size();

				while (text_ptr < text_end)
				{
					unsigned int codepoint = 0;
					int bytes_consumed = ImTextCharFromUtf8(&codepoint, text_ptr, text_end);
					if (bytes_consumed <= 0)
						break;
					text_ptr += bytes_consumed;
					if (codepoint == '\n')
					{
						row++;
						col = 0;
						continue;
					}

					float offset_x = (glitch_intensity > 0.0f) ? jitter_dist(main_rng) * canvas_scale : 0.0f;
					float offset_y = (glitch_intensity > 0.0f) ? jitter_dist(main_rng) * canvas_scale : 0.0f;

					ImVec2 char_pos(
						canvas_pos.x + (col * char_spacing_x * canvas_scale) + offset_x,
						canvas_pos.y + (row * char_spacing_y * canvas_scale) + offset_y);

					char buf[5] = {0};
					ImTextCharToUtf8(buf, codepoint);
					draw_list->AddText(fonts.ascii_font, ascii_font_size * canvas_scale, char_pos, txt_col, buf);

					col++;
				}
			}
			ImGui::Dummy(ImVec2(canvas_w, canvas_h));
		}
		else
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Load an image to begin tuning aspect ratios and spacing.");
		}
		ImGui::End();

		ImGui::Render();

		present_imgui_frame(window);

		if (pending_image_export)
		{
			if (!export_to_image(
				image_export_path,
				displayed_ascii,
				fonts.ascii_font,
				ascii_font_size,
				char_spacing_x,
				char_spacing_y,
				bg_color,
				text_color,
				glitch_intensity,
					export_full_canvas,
					current_viewport_size,
					viewport_zoom,
					current_viewport_scroll))
			{
				std::cerr << "Failed to export image: " << image_export_path << '\n';
			}
			pending_image_export = false;
		}
			if (pending_text_export)
		{
			if (!export_to_text(text_export_path, displayed_ascii))
				std::cerr << "Failed to export text: " << text_export_path << '\n';
				pending_text_export = false;
			}
			if (pending_gif_frame)
			{
				std::array<char, 32> frame_name{};
				std::snprintf(
					frame_name.data(), frame_name.size(),
					"frame_%02d.png", gif_frame_count + 1);
				const fs::path frame_path = gif_temp_directory / frame_name.data();
				const bool frame_exported = export_to_image(
					frame_path.string(), displayed_ascii, fonts.ascii_font, ascii_font_size,
					char_spacing_x, char_spacing_y, bg_color, text_color, glitch_intensity,
					gif_export_area == 1, current_viewport_size, viewport_zoom,
					current_viewport_scroll);
				if (frame_exported)
				{
					++gif_frame_count;
					gif_status = "Frame " + std::to_string(gif_frame_count) + " added.";
				}
				else
				{
					gif_status = "Failed to render GIF frame.";
				}
				pending_gif_frame = false;
			}
			if (pending_gif_generation)
			{
				const std::string command =
					"/bin/bash " + shell_quote(gif_script_path) + " " +
					shell_quote(gif_output_path) + " " + std::to_string(gif_delay) + " " +
					shell_quote(gif_temp_directory);
				const int result = std::system(command.c_str());
				if (result == 0)
				{
					std::error_code error;
					fs::remove_all(gif_temp_directory, error);
					gif_temp_directory.clear();
					gif_frame_count = 0;
					gif_status = error
						? "GIF created, but temporary frames could not be fully removed."
						: "GIF created: " + gif_output_path;
				}
				else
				{
					gif_status = "GIF generation failed; temporary frames were preserved.";
				}
				pending_gif_generation = false;
			}
			if (pending_font_rebuild)
		{
			fonts = rebuild_font_atlas(builtin_ascii_fonts[selected_ascii_font], ascii_font_size);
			pending_font_rebuild = false;
		}

	}

	if (!gif_temp_directory.empty())
	{
		std::error_code error;
		fs::remove_all(gif_temp_directory, error);
	}
	destroy_image_buffer(current_img);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
