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
#include "AppState.h"
#include "ImageProcess.h"
#include "RampEditor.h"
#include "TextEditor.h"
#include "TextLayout.h"

using namespace AsciiArt;
namespace fs = std::filesystem;

AppState::State app;

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

static void rebuild_processed_base_texture(AppState::ImageState &image,
	const ImageProcess::Settings &settings)
{
	if (image.processed_texture != 0)
	{
		glDeleteTextures(1, &image.processed_texture);
		image.processed_texture = 0;
	}
	const ImageProcess::ProcessedImage processed = ImageProcess::apply_to_image(
		image.current.pixels, image.current.width, image.current.height,
		image.current.channels, settings);
	if (!processed.pixels.empty())
	{
		image.processed_texture = create_texture_from_pixels(
			processed.pixels.data(), processed.width, processed.height, processed.channels);
	}
}

int main(int argc, char **argv)
{
	const fs::path executable_path = argc > 0 ? fs::absolute(argv[0]) : fs::current_path() / "image_character";
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

	auto &current_img = app.image.current;
	auto &current_file_path = app.image.file_path;
	auto &ascii_art = app.image.ascii;
	auto &ramp_editor = app.ramp.editor;
	RampEditor::reset(ramp_editor, Config::default_ramp);
	auto &ramp_buffer = ramp_editor.buffer;
	app.generation.ramp = ramp_buffer.data();
	auto &selected_ascii_font = app.font.selected;
	auto &ascii_font_size = app.image_export.font_size;
	auto &pending_font_rebuild = app.font.pending_rebuild;
	auto &fonts = app.font.fonts;
	fonts = rebuild_font_atlas(builtin_ascii_fonts[selected_ascii_font], ascii_font_size);
	app.image_export.font = fonts.ascii_font;
	auto &char_spacing_x = app.image_export.character_spacing.x;
	auto &char_spacing_y = app.image_export.character_spacing.y;
	auto &target_columns = app.generation.columns;
	auto &target_rows = app.generation.rows;
	auto &lock_aspect = app.font.lock_aspect;

	using namespace ImageProcess;
	auto &processing = app.generation.processing;
	auto &brightness = processing.brightness;
	auto &contrast = processing.contrast;
	auto &grayscale = processing.grayscale;
	auto &invert = processing.invert;
	auto &edge_detection = processing.edge_detection;
	auto &edge_detector = processing.edge_detector;
	auto &edge_style = processing.edge_style;
	auto &edge_threshold = processing.edge_threshold;
	auto &edge_characters = processing.edge_characters;
	auto &simple_edge_buffer = app.ramp.simple_edge;
	auto &horizontal_edge_buffer = app.ramp.horizontal_edge;
	auto &vertical_edge_buffer = app.ramp.vertical_edge;
	auto &rising_edge_buffer = app.ramp.rising_edge;
	auto &falling_edge_buffer = app.ramp.falling_edge;
	std::snprintf(horizontal_edge_buffer.data(), horizontal_edge_buffer.size(), "%s", "─");
	std::snprintf(vertical_edge_buffer.data(), vertical_edge_buffer.size(), "%s", "│");
	std::snprintf(rising_edge_buffer.data(), rising_edge_buffer.size(), "%s", "╱");
	std::snprintf(falling_edge_buffer.data(), falling_edge_buffer.size(), "%s", "╲");
	auto &base_opacity = app.base_opacity;
	auto &show_base_layer = app.show_base_layer;
	auto &show_processed_base = app.show_processed_base;
	auto &show_ascii_layer = app.show_ascii_layer;
	auto &text_color = app.image_export.text_color;
	auto &bg_color = app.image_export.background_color;
	auto &glitch_intensity = app.image_export.glitch_intensity;
	auto &glitch_per_frame = app.glitch.animate;
	auto &main_rng = app.glitch.random;
	auto &export_full_canvas = app.image_export.full_canvas;
	auto &current_viewport_size = app.viewport_transform.size;
	auto &current_viewport_scroll = app.viewport_transform.scroll;
	auto &viewport_zoom = app.viewport_transform.zoom;
	auto &text_editor = app.viewport.editor;
	auto &preferred_editor_height = app.viewport.preferred_editor_height;
	auto &control_panel_collapsed = app.viewport.control_panel_collapsed;
	auto &pending_image_export = app.export_job.pending_image;
	auto &image_export_path = app.export_job.image_path;
	auto &pending_text_export = app.export_job.pending_text;
	auto &text_export_path = app.export_job.text_path;
	auto &gif_temp_directory = app.gif.temporary_directory;
	auto &gif_frame_count = app.gif.frame_count;
	auto &gif_delay = app.gif.delay;
	auto &gif_export_area = app.gif.export_area;
	auto &pending_gif_frame = app.gif.pending_frame;
	auto &pending_gif_generation = app.gif.pending_generation;
	auto &gif_output_path = app.gif.output_path;
	auto &gif_status = app.gif.status;

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
		ImGui::Begin("Matrix Controls", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

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
						rebuild_processed_base_texture(app.image, processing);

						target_columns = 80;
						target_rows = calculate_locked_rows(current_img, target_columns, char_spacing_x, char_spacing_y);

						ascii_art = generate_ascii(current_img, app.generation);
						app.image.generated_layout = Text::build_layout(
							ascii_art.text, false, std::max(1, ascii_art.cols));
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
			{
				ascii_art = generate_ascii(current_img, app.generation);
				app.image.generated_layout = Text::build_layout(
					ascii_art.text, false, std::max(1, ascii_art.cols));
			}

			ImGui::Separator();
			ImGui::Text("3. Image Processing");
			const ImageProcess::Settings previous_processing = processing;
			bool processing_changed = false;
			processing_changed |= ImGui::Checkbox("Invert Image Colors", &invert);
			processing_changed |= ImGui::SliderFloat("Brightness", &brightness, -100.0f, 100.0f);
			processing_changed |= ImGui::SliderFloat("Contrast", &contrast, 0.1f, 3.0f);
			processing_changed |= ImGui::SliderFloat("Grayscale", &grayscale, 0.0f, 1.0f, "%.2f");
			if (ImGui::Button(edge_detection ? "Disable Edge Detection" : "Enable Edge Detection", ImVec2(-1.0f, 0.0f)))
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
				processing_changed |= ImGui::SliderFloat("Edge Threshold", &edge_threshold, 0.0f, 255.0f, "%.0f");
			}
			ImGui::Checkbox("Show ASCII Overlay", &show_ascii_layer);
			ImGui::SliderFloat("Base Layer Opacity", &base_opacity, 0.0f, 1.0f);
			ImGui::Checkbox("Show Base Image", &show_base_layer);
			ImGui::BeginDisabled(!show_base_layer);
			if (ImGui::Checkbox("Apply Processing to Base Image", &show_processed_base) &&
				show_processed_base && current_img.pixels)
			{
				rebuild_processed_base_texture(app.image, processing);
			}
			ImGui::EndDisabled();

			ImGui::Separator();
			ImGui::Text("4. Ramp Characters");
			if (ImGui::Button("Default Ramp"))
			{
				RampEditor::reset(ramp_editor, Config::default_ramp);
				processing_changed = true;
			}
			if (ImGui::InputText("Ramp", ramp_buffer.data(), ramp_buffer.size(),
				ImGuiInputTextFlags_CallbackAlways, RampEditor::capture_cursor, &ramp_editor))
				processing_changed = true;
			if (ImGui::Button("Reverse Ramp", ImVec2(-1.0f, 0.0f)))
			{
				RampEditor::reverse(ramp_editor);
				processing_changed = true;
			}

			if (ImGui::TreeNodeEx("Special Symbol Keyboard", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth))
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
							if (RampEditor::insert(ramp_editor,
								std::string_view(symbol_cursor, static_cast<std::size_t>(byte_count))))
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
					ascii_art = generate_ascii(current_img, app.generation);
					app.image.generated_layout = Text::build_layout(
						ascii_art.text, false, std::max(1, ascii_art.cols));
					if (show_processed_base && !(processing == previous_processing))
						rebuild_processed_base_texture(app.image, processing);
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
			const bool spacing_x_changed = ImGui::DragFloat("Horizontal Spacing", &char_spacing_x, 0.1f, 0.1f, 100.0f, "%.1f");
			const bool spacing_y_changed = ImGui::DragFloat("Vertical Spacing", &char_spacing_y, 0.1f, 0.1f, 100.0f, "%.1f");

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
			ImGui::Text("7. Color ");
			ImGui::ColorEdit4("Canvas Background", (float *)&bg_color);
			ImGui::ColorEdit4("Text Color", (float *)&text_color);

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

			ImGui::BeginDisabled(gif_frame_count >= 16 || ascii_art.text.empty() || pending_gif_frame);
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
		ImGui::SetNextWindowSize(ImVec2(viewport_width, tools_height), ImGuiCond_Always);
		ImGui::Begin("Viewport Tools",nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

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
			TextEditor::set_text(text_editor, ascii_art.text);
		ImGui::SameLine();
		if (ImGui::Checkbox("Soft Wrap", &text_editor.soft_wrap))
			TextEditor::invalidate_layout(text_editor);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.0f);
		if (ImGui::DragFloat("Editor Width", &text_editor.width, 2.0f, 120.0f, 8000.0f, "%.0f px"))
			TextEditor::invalidate_layout(text_editor);
		text_editor.width = std::clamp(text_editor.width, 120.0f, 8000.0f);
		ImGui::End();

		if (text_editor.enabled && !text_editor.modified && std::strcmp(text_editor.buffer.data(), ascii_art.text.c_str()) != 0)
			TextEditor::set_text(text_editor, ascii_art.text);

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
				TextEditor::set_text(text_editor, ascii_art.text);
			}
			ImGui::SameLine();
			ImGui::TextDisabled("Edits are preserved until Reset from Generated is pressed.");

			const int editor_wrap_columns = std::max(1, static_cast<int>(
				text_editor.width / std::max(0.1f, char_spacing_x * viewport_zoom)));
			const Text::Layout &editor_layout = TextEditor::update_layout(
				text_editor, editor_wrap_columns);
			const float editor_content_width = std::max(text_editor.width,
				editor_layout.maximum_source_columns * char_spacing_x * viewport_zoom);
			const float editor_footer_height = ImGui::GetTextLineHeightWithSpacing();
			const float input_height = std::max(1.0f, ImGui::GetContentRegionAvail().y - editor_footer_height);
			ImGui::PushFont(fonts.ascii_font, ascii_font_size * viewport_zoom);

			const bool editor_changed = ImGui::InputTextMultiline("##AsciiTextEditor", text_editor.buffer.data(), text_editor.buffer.size(),
				ImVec2(std::max(120.0f, editor_content_width), input_height), ImGuiInputTextFlags_AllowTabInput);

			ImGui::PopFont();
			if (editor_changed)
			{
				text_editor.modified = true;
				TextEditor::invalidate_layout(text_editor);
			}
		}
		ImGui::End();

		const int editor_wrap_columns = std::max(1, static_cast<int>(text_editor.width / std::max(0.1f, char_spacing_x * viewport_zoom)));
		const Text::Layout &display_layout = text_editor.enabled
			? TextEditor::update_layout(text_editor, editor_wrap_columns)
			: app.image.generated_layout;
		const AsciiOutput &displayed_ascii = display_layout.output;

		// Canvas viewport
		const float canvas_y = canvas_was_collapsed ? std::max(tools_height, io.DisplaySize.y - collapsed_bar_height) : tools_height + editor_layout_height;
		ImGui::SetNextWindowPos(ImVec2(viewport_x, canvas_y), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(viewport_width, std::max(1.0f, io.DisplaySize.y - canvas_y)), ImGuiCond_Always);
		const bool canvas_expanded = ImGui::Begin("Canvas Viewport", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_HorizontalScrollbar);

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
				const GLuint base_texture = show_processed_base && app.image.processed_texture != 0
					? app.image.processed_texture
					: current_img.textureID;
				draw_list->AddImage((void *)(intptr_t)base_texture, canvas_pos,
					ImVec2(canvas_pos.x + unscaled_base_width * canvas_scale,
						canvas_pos.y + unscaled_base_height * canvas_scale),
					ImVec2(0, 0), ImVec2(1, 1), tint);
			}

			if (show_ascii_layer && !displayed_ascii.text.empty())
			{
				ImU32 txt_col = ImGui::ColorConvertFloat4ToU32(text_color);

				if (!glitch_per_frame)
					main_rng.seed(1337);
				std::uniform_real_distribution<float> jitter_dist(-glitch_intensity, glitch_intensity);

				for (const Text::Glyph &glyph : display_layout.glyphs)
				{
					float offset_x = (glitch_intensity > 0.0f) ? jitter_dist(main_rng) * canvas_scale : 0.0f;
					float offset_y = (glitch_intensity > 0.0f) ? jitter_dist(main_rng) * canvas_scale : 0.0f;

					ImVec2 char_pos(
						canvas_pos.x + (glyph.column * char_spacing_x * canvas_scale) + offset_x,
						canvas_pos.y + (glyph.row * char_spacing_y * canvas_scale) + offset_y);

					char buf[5] = {0};
					ImTextCharToUtf8(buf, glyph.codepoint);
					draw_list->AddText(fonts.ascii_font, ascii_font_size * canvas_scale, char_pos, txt_col, buf);
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
				image_export_path, displayed_ascii, app.image_export, app.viewport_transform))
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
			std::snprintf(frame_name.data(), frame_name.size(), "frame_%02d.png", gif_frame_count + 1);
			const fs::path frame_path = gif_temp_directory / frame_name.data();
			ImageExportOptions gif_options = app.image_export;
			gif_options.full_canvas = gif_export_area == 1;
			const bool frame_exported = export_to_image(
				frame_path.string(), displayed_ascii, gif_options, app.viewport_transform);
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
			const std::string command = "/bin/bash " + shell_quote(gif_script_path) + " " + shell_quote(gif_output_path) + " " + std::to_string(gif_delay) + " " + shell_quote(gif_temp_directory);
			const int result = std::system(command.c_str());
			if (result == 0)
			{
				std::error_code error;
				fs::remove_all(gif_temp_directory, error);
				gif_temp_directory.clear();
				gif_frame_count = 0;
				gif_status = error ? "GIF created, but temporary frames could not be fully removed." : "GIF created: " + gif_output_path;
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
			app.image_export.font = fonts.ascii_font;
			pending_font_rebuild = false;
		}
	}

	if (!gif_temp_directory.empty())
	{
		std::error_code error;
		fs::remove_all(gif_temp_directory, error);
	}

	if (app.image.processed_texture != 0)
		glDeleteTextures(1, &app.image.processed_texture);
	destroy_image_buffer(current_img);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
