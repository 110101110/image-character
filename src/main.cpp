#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <algorithm>
#include <random>

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

struct RampEditorState
{
	std::size_t cursor_byte = 0;
};

static int capture_ramp_cursor(ImGuiInputTextCallbackData *data)
{
	auto *state = static_cast<RampEditorState *>(data->UserData);
	state->cursor_byte = static_cast<std::size_t>(std::max(0, data->CursorPos));
	return 0;
}

static bool insert_utf8_at_cursor(
	char *buffer,
	std::size_t capacity,
	std::size_t &cursor_byte,
	const char *character,
	std::size_t character_size)
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

int main()
{
	if (!glfwInit())
		return -1;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow *window = glfwCreateWindow(
		Config::window_width,
		Config::window_height,
		Config::window_title,
		nullptr,
		nullptr);
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
	bool invert_image = false;
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
	float viewport_zoom = 1.0f;

	bool pending_image_export = false;
	std::string image_export_path;
	bool pending_text_export = false;
	std::string text_export_path;

	std::mt19937 main_rng(1337);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, true);
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(Config::sidebar_width, io.DisplaySize.y), ImGuiCond_Always);
		ImGui::Begin(
			"Matrix Controls",
			nullptr,
			ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoScrollbar);

		ImGui::Text("1. Reference Image");
		if (ImGui::Button("Open File ...", ImVec2(-1, 30)))
		{
			std::string selected_path = OpenNativeImageDialog();
			if (!selected_path.empty())
			{
				current_file_path = selected_path;
				int w, h, ch;
				unsigned char *data = stbi_load(current_file_path.c_str(), &w, &h, &ch, 4);
				if (data)
				{
					destroy_image_buffer(current_img);

					current_img.pixels = data;
					current_img.width = w;
					current_img.height = h;
					current_img.channels = 4;
					current_img.textureID = create_texture_from_pixels(data, w, h, 4);

					target_columns = 80;
					target_rows = calculate_locked_rows(
						current_img,
						target_columns,
						char_spacing_x,
						char_spacing_y);

					ascii_art = generate_ascii(current_img, target_columns, target_rows, brightness, contrast, invert_image, ramp_buffer.data());
				}
			}
		}
		if (!current_file_path.empty())
		{
			ImGui::TextWrapped("File: %s", current_file_path.c_str());
		}

		ImGui::Separator();
		ImGui::Text("2. Image Grid & Ratio Controls");
		const bool aspect_lock_changed = ImGui::Checkbox("Lock Aspect Ratio", &lock_aspect);
		if (aspect_lock_changed && lock_aspect && current_img.pixels)
		{
			char_spacing_y = calculate_locked_spacing_y(
				current_img, target_columns, target_rows, char_spacing_x);
		}
		ImGui::Text("Grid Resolution: %d x %d", ascii_art.cols, ascii_art.rows);

		bool grid_changed = false;
		if (ImGui::DragInt("Columns (X)", &target_columns, 1, 10, 500))
		{
			grid_changed = true;
			if (lock_aspect && current_img.height > 0 && current_img.width > 0)
			{
				target_rows = calculate_locked_rows(
					current_img,
					target_columns,
					char_spacing_x,
					char_spacing_y);
			}
		}
		if (ImGui::DragInt("Rows (Y)", &target_rows, 1, 10, 500))
		{
			grid_changed = true;
			if (lock_aspect && current_img.height > 0 && current_img.width > 0)
			{
				target_columns = calculate_locked_columns(
					current_img,
					target_rows,
					char_spacing_x,
					char_spacing_y);
			}
		}
		if (grid_changed && current_img.pixels)
		{
			ascii_art = generate_ascii(current_img, target_columns, target_rows, brightness, contrast, invert_image, ramp_buffer.data());
		}

		ImGui::Separator();
		ImGui::Text("3. Image Processing");
		bool processing_changed = false;
		processing_changed |= ImGui::Checkbox("Invert Image Colors", &invert_image);
		processing_changed |= ImGui::SliderFloat("Brightness", &brightness, -100.0f, 100.0f);
		processing_changed |= ImGui::SliderFloat("Contrast", &contrast, 0.1f, 3.0f);

		ImGui::Separator();
		ImGui::Text("4. Ramp Characters");
		if (ImGui::InputText(
			"Ramp",
			ramp_buffer.data(),
			ramp_buffer.size(),
			ImGuiInputTextFlags_CallbackAlways,
			capture_ramp_cursor,
			&ramp_editor))
		{
			processing_changed = true;
		}

		if (ImGui::Button("Reverse Ramp", ImVec2(-1.0f, 0.0f)))
		{
			const std::string reversed = reverse_utf8(ramp_buffer.data());
			std::memcpy(ramp_buffer.data(), reversed.c_str(), reversed.size() + 1);
			ramp_editor.cursor_byte = reversed.size();
			processing_changed = true;
		}

		if (ImGui::CollapsingHeader("Special Symbol Keyboard", ImGuiTreeNodeFlags_DefaultOpen))
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
		}

		if (processing_changed)
		{
			if (current_img.pixels)
			{
				ascii_art = generate_ascii(
					current_img, target_columns, target_rows, brightness, contrast, invert_image, ramp_buffer.data());
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
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}
		ImGui::DragFloat(
			"Font Size", &ascii_font_size,0.1f, Config::minimum_ascii_font_size,Config::maximum_ascii_font_size, "%.1f");
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			pending_font_rebuild = true;
		}
		const bool spacing_x_changed = ImGui::DragFloat(
			"Horizontal Spacing", &char_spacing_x, 0.1f, 0.1f, 100.0f, "%.1f");
		const bool spacing_y_changed = ImGui::DragFloat(
			"Vertical Spacing", &char_spacing_y, 0.1f, 0.1f, 100.0f, "%.1f");

		if (lock_aspect && current_img.pixels)
		{
			if (spacing_x_changed)
			{
				char_spacing_y = calculate_locked_spacing_y(
					current_img, target_columns, target_rows, char_spacing_x);
			}
			else if (spacing_y_changed)
			{
				char_spacing_x = calculate_locked_spacing_x(
					current_img, target_columns, target_rows, char_spacing_y);
			}
		}

		ImGui::Separator();
		ImGui::Text("6. Glitch Mechanics");
		ImGui::SliderFloat("Glitch Jitter", &glitch_intensity, 0.0f, 50.0f);
		ImGui::Checkbox("Animate Glitch", &glitch_per_frame);

		ImGui::Separator();
		ImGui::Text("7. Color & Layer Opacity");
		ImGui::ColorEdit4("Canvas Background", (float *)&bg_color);
		ImGui::ColorEdit4("Text Color", (float *)&text_color);
		ImGui::Checkbox("Show Base Image", &show_base_layer);
		ImGui::SliderFloat("Base Layer Opacity", &base_opacity, 0.0f, 1.0f);
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
		ImGui::End();

		// Viewport
		const float viewport_x = Config::sidebar_width;
		ImGui::SetNextWindowPos(ImVec2(viewport_x, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(
			ImVec2(std::max(1.0f, io.DisplaySize.x - viewport_x), io.DisplaySize.y),
			ImGuiCond_Always);
		ImGui::Begin(
			"Canvas Viewport",
			nullptr,
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_HorizontalScrollbar);

		const bool viewport_hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		if (viewport_hovered && (io.KeyCtrl || io.KeySuper) && io.MouseWheel != 0.0f)
		{
			viewport_zoom = std::clamp(
				viewport_zoom * (io.MouseWheel > 0.0f ? 1.1f : 1.0f / 1.1f),
				0.1f,
				8.0f);
		}

		ImGui::SetNextItemWidth(200.0f);
		ImGui::SliderFloat("Zoom", &viewport_zoom, 0.1f, 8.0f, "%.2fx", ImGuiSliderFlags_Logarithmic);
		viewport_zoom = std::clamp(viewport_zoom, 0.1f, 8.0f);
		ImGui::SameLine();
		if (ImGui::Button("100%"))
			viewport_zoom = 1.0f;
		ImGui::SameLine();
		ImGui::TextDisabled("Ctrl/Cmd + wheel");
		ImGui::Separator();

		current_viewport_size = ImGui::GetContentRegionAvail();
		ImDrawList *draw_list = ImGui::GetWindowDrawList();
		ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

		if (current_img.textureID > 0)
		{
			const float unscaled_canvas_w = ascii_art.cols * char_spacing_x;
			const float unscaled_canvas_h = ascii_art.rows * char_spacing_y;
			const float canvas_scale = viewport_zoom;
			const float canvas_w = unscaled_canvas_w * canvas_scale;
			const float canvas_h = unscaled_canvas_h * canvas_scale;

			if (show_base_layer)
			{
				ImU32 tint = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, base_opacity));
				draw_list->AddImage(
					(void *)(intptr_t)current_img.textureID,
					canvas_pos,
					ImVec2(canvas_pos.x + canvas_w, canvas_pos.y + canvas_h),
					ImVec2(0, 0), ImVec2(1, 1),
					tint);
			}

			if (show_ascii_layer && !ascii_art.text.empty())
			{
				ImU32 txt_col = ImGui::ColorConvertFloat4ToU32(text_color);

				if (!glitch_per_frame)
				{
					main_rng.seed(1337);
				}
				std::uniform_real_distribution<float> jitter_dist(-glitch_intensity, glitch_intensity);

				int col = 0;
				int row = 0;

				const char *text_ptr = ascii_art.text.c_str();
				const char *text_end = text_ptr + ascii_art.text.size();

				//rendering loop with extended unicode
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
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Load an image to begin tuning aspect ratios and spacing.");
		}
		ImGui::End();

		ImGui::Render();

		present_imgui_frame(window);

		if (pending_image_export)
		{
			if (!export_to_image(
				image_export_path,
				ascii_art,
				fonts.ascii_font,
				ascii_font_size,
				char_spacing_x,
				char_spacing_y,
				bg_color,
				text_color,
				glitch_intensity,
				export_full_canvas,
				current_viewport_size))
			{
				std::cerr << "Failed to export image: " << image_export_path << '\n';
			}
			pending_image_export = false;
		}
		if (pending_text_export)
		{
			if (!export_to_text(text_export_path, ascii_art))
				std::cerr << "Failed to export text: " << text_export_path << '\n';
			pending_text_export = false;
		}
		if (pending_font_rebuild)
		{
			fonts = rebuild_font_atlas(builtin_ascii_fonts[selected_ascii_font], ascii_font_size);
			pending_font_rebuild = false;
		}

	}

	destroy_image_buffer(current_img);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
