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

int main()
{
	if (!glfwInit())
		return -1;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

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
	glfwSetWindowAspectRatio(window, 16, 9);
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

	ImageBuffer current_img;
	std::string current_file_path = "";
	AsciiOutput ascii_art;

	char ramp_buffer[128] = " .:-=+*#%@";

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

	bool pending_export = false;
	std::string export_file_path = "";

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
		ImGui::Begin("Glitch & Matrix Controls", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

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

					ascii_art = generate_ascii(current_img, target_columns, target_rows, brightness, contrast, invert_image,ramp_buffer);
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
			ascii_art = generate_ascii(current_img, target_columns, target_rows, brightness, contrast, invert_image,ramp_buffer);
		}

		ImGui::Separator();
		ImGui::Text("3. Image Processing");
		if (ImGui::Checkbox("Invert Image Colors", &invert_image) ||
			ImGui::SliderFloat("Brightness", &brightness, -100.0f, 100.0f) ||
			ImGui::SliderFloat("Contrast", &contrast, 0.1f, 3.0f) ||
			ImGui::InputText("Ramp String", ramp_buffer, IM_ARRAYSIZE(ramp_buffer)))
		{
			if (current_img.pixels)
			{
				ascii_art = generate_ascii(current_img, target_columns, target_rows, brightness, contrast, invert_image, ramp_buffer);
			}
		}

		ImGui::Separator();
		ImGui::Text("4. Character Setting");
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
		ImGui::Text("5.Glitch Mechanics");
		ImGui::SliderFloat("Glitch Jitter", &glitch_intensity, 0.0f, 50.0f);
		ImGui::Checkbox("Animate Glitch", &glitch_per_frame);

		ImGui::Separator();
		ImGui::Text("6. Color & Layer Opacity");
		ImGui::ColorEdit4("Canvas Background", (float *)&bg_color);
		ImGui::ColorEdit4("Text Color", (float *)&text_color);
		ImGui::Checkbox("Show Base Image", &show_base_layer);
		ImGui::SliderFloat("Base Layer Opacity", &base_opacity, 0.0f, 1.0f);
		ImGui::Checkbox("Show ASCII Overlay", &show_ascii_layer);

		ImGui::Separator();
		ImGui::Text("7. Exports");
		ImGui::Checkbox("Export Entire ASCII Canvas", &export_full_canvas);
		if (ImGui::Button("Export Image (.PNG / .JPG)", ImVec2(-1, 30)))
		{
			std::string save_path = SaveNativeFileDialog("ascii_art.png");
			if (!save_path.empty())
			{
				export_file_path = save_path;
				pending_export = true;
			}
		}
		ImGui::End();

		// Viewport
		ImGui::SetNextWindowPos(ImVec2(Config::sidebar_width, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(
			ImVec2(io.DisplaySize.x - Config::sidebar_width, io.DisplaySize.y),
			ImGuiCond_Always);
		ImGui::Begin("Canvas Viewport", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar);

		current_viewport_size = ImGui::GetContentRegionAvail();
		ImDrawList *draw_list = ImGui::GetWindowDrawList();
		ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

		if (current_img.textureID > 0)
		{
			float canvas_w = ascii_art.cols * char_spacing_x;
			float canvas_h = ascii_art.rows * char_spacing_y;

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

					float offset_x = (glitch_intensity > 0.0f) ? jitter_dist(main_rng) : 0.0f;
					float offset_y = (glitch_intensity > 0.0f) ? jitter_dist(main_rng) : 0.0f;

					ImVec2 char_pos(
						canvas_pos.x + (col * char_spacing_x) + offset_x,
						canvas_pos.y + (row * char_spacing_y) + offset_y);

					char buf[5] = {0};
					ImTextCharToUtf8(buf, codepoint);
					draw_list->AddText(fonts.ascii_font, ascii_font_size, char_pos, txt_col, buf);

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

		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (pending_export)
		{
			export_to_image(
				export_file_path,
				ascii_art,
				fonts.ascii_font,
				ascii_font_size,
				char_spacing_x,
				char_spacing_y,
				bg_color,
				text_color,
				glitch_intensity,
				export_full_canvas,
				current_viewport_size);
			pending_export = false;
		}
		if (pending_font_rebuild)
		{
			fonts = rebuild_font_atlas(builtin_ascii_fonts[selected_ascii_font], ascii_font_size);
			pending_font_rebuild = false;
		}

		glfwSwapBuffers(window);
	}

	destroy_image_buffer(current_img);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
