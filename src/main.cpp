#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>

#include <glad/glad.h>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "NativeFileDialog.h"

struct ImageBuffer
{
	unsigned char *pixels = nullptr;
	int width = 0;
	int height = 0;
	int channels = 0;
	GLuint textureID = 0;
};

struct AsciiOutput
{
	std::string text;
	int cols = 0;
	int rows = 0;
};

//the rendering function
AsciiOutput generate_ascii(const ImageBuffer &img, int target_cols, int target_rows,
						   float brightness, float contrast, bool invert, const std::string &ramp)
{
	AsciiOutput out;
	if (!img.pixels || img.width <= 0 || img.height <= 0 || target_cols <= 0 || target_rows <= 0)
		return out;

	int ramp_len = static_cast<int>(ramp.length());
	if (ramp_len == 0)
		return out;

	out.cols = target_cols;
	out.rows = target_rows;
	out.text.reserve((target_cols + 1) * target_rows);

	float cell_w = static_cast<float>(img.width) / target_cols;
	float cell_h = static_cast<float>(img.height) / target_rows;

	for (int r = 0; r < target_rows; ++r)
	{
		for (int c = 0; c < target_cols; ++c)
		{
			int start_x = static_cast<int>(c * cell_w);
			int start_y = static_cast<int>(r * cell_h);
			int end_x = std::min(static_cast<int>((c + 1) * cell_w), img.width);
			int end_y = std::min(static_cast<int>((r + 1) * cell_h), img.height);

			float total_lum = 0.0f;
			int pixel_count = 0;

			for (int y = start_y; y < end_y; ++y)
			{
				for (int x = start_x; x < end_x; ++x)
				{
					int idx = (y * img.width + x) * img.channels;
					float red = img.pixels[idx + 0];
					float green = img.pixels[idx + 1];
					float blue = img.pixels[idx + 2];

					// Rec. 709 Luminance [cite: 11]
					float lum = 0.2126f * red + 0.7152f * green + 0.0722f * blue;
					if (invert)
					{
						lum = 255.0f - lum;
					}
					total_lum += lum;
					pixel_count++;
				}
			}

			float avg_lum = (pixel_count > 0) ? (total_lum / pixel_count) : 0.0f;
			avg_lum = (avg_lum - 128.0f) * contrast + 128.0f + brightness;
			avg_lum = std::clamp(avg_lum, 0.0f, 255.0f);

			int ramp_idx = static_cast<int>((avg_lum / 255.0f) * (ramp_len - 1));
			out.text += ramp[ramp_idx];
		}
		out.text += '\n';
	}

	return out;
}

GLuint create_texture_from_pixels(const unsigned char *pixels, int w, int h, int channels)
{
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
	glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	return tex;
}

//function to export ascii layer to image file
void export_ascii_fbo_gpu(const std::string &filepath, const AsciiOutput &ascii,
						  float spacing_x, float spacing_y,
						  ImVec4 bg_col, ImVec4 text_col, float glitch_intensity,
						  bool export_full_canvas, ImVec2 viewport_size)
{
	if (filepath.empty() || ascii.text.empty() || ascii.cols <= 0 || ascii.rows <= 0)
		return;

	// 1. Calculate Off-Screen Canvas Dimensions
	int export_w = export_full_canvas ? static_cast<int>(ascii.cols * spacing_x)
									  : static_cast<int>(viewport_size.x);
	int export_h = export_full_canvas ? static_cast<int>(ascii.rows * spacing_y)
									  : static_cast<int>(viewport_size.y);

	if (export_w <= 0 || export_h <= 0)
		return;

	// Save previous active OpenGL viewport and framebuffer state
	GLint prev_fbo = 0;
	GLint prev_viewport[4];
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
	glGetIntegerv(GL_VIEWPORT, prev_viewport);

	// 2. Create Off-Screen Framebuffer and Color Texture Target
	GLuint fbo, tex;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, export_w, export_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
		glDeleteTextures(1, &tex);
		glDeleteFramebuffers(1, &fbo);
		return;
	}

	// 3. Clear Off-Screen Framebuffer with Canvas Background Color
	glViewport(0, 0, export_w, export_h);
	glClearColor(bg_col.x, bg_col.y, bg_col.z, bg_col.w);
	glClear(GL_COLOR_BUFFER_BIT);

	// 4. Render ASCII Layer directly using ImGui DrawList into the Off-Screen Context
	ImGui::NewFrame();
	ImDrawList *draw_list = ImGui::GetForegroundDrawList();
	ImU32 txt_col_u32 = ImGui::ColorConvertFloat4ToU32(text_col);
	ImFont *font = ImGui::GetFont();
	float font_size = ImGui::GetFontSize();

	std::mt19937 rng(1337);
	std::uniform_real_distribution<float> jitter_dist(-glitch_intensity, glitch_intensity);

	int col = 0, row = 0;
	for (char ch : ascii.text)
	{
		if (ch == '\n')
		{
			row++;
			col = 0;
			continue;
		}

		float offset_x = (glitch_intensity > 0.0f) ? jitter_dist(rng) : 0.0f;
		float offset_y = (glitch_intensity > 0.0f) ? jitter_dist(rng) : 0.0f;

		// Render relative to off-screen origin (0, 0)
		ImVec2 char_pos(
			(col * spacing_x) + offset_x,
			(row * spacing_y) + offset_y);

		char buf[2] = {ch, '\0'};
		draw_list->AddText(font, font_size, char_pos, txt_col_u32, buf);
		col++;
	}

	// Process draw calls for the off-screen frame
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// 5. Read back pixel memory directly from the GPU Framebuffer
	std::vector<unsigned char> pixel_buffer(export_w * export_h * 4);
	glReadPixels(0, 0, export_w, export_h, GL_RGBA, GL_UNSIGNED_BYTE, pixel_buffer.data());

	// Restore previous OpenGL state
	glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
	glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);

	glDeleteTextures(1, &tex);
	glDeleteFramebuffers(1, &fbo);

	// 6. Flip Y-axis orientation for image file formats (OpenGL vs Image File Origin)
	std::vector<unsigned char> flipped_buffer(export_w * export_h * 4);
	for (int y = 0; y < export_h; ++y)
	{
		memcpy(&flipped_buffer[y * export_w * 4],
			   &pixel_buffer[(export_h - 1 - y) * export_w * 4],
			   export_w * 4);
	}

	// 7. Write directly to PNG or JPG via stb_image_write
	if (filepath.ends_with(".jpg") || filepath.ends_with(".jpeg"))
	{
		stbi_write_jpg(filepath.c_str(), export_w, export_h, 4, flipped_buffer.data(), 90);
	}
	else
	{
		stbi_write_png(filepath.c_str(), export_w, export_h, 4, flipped_buffer.data(), export_w * 4);
	}
}

int main()
{
	if (!glfwInit())
		return -1;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	GLFWwindow *window = glfwCreateWindow(1400, 850, "ASCII Art", nullptr, nullptr);
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
	(void)io;
	ImGui::StyleColorsLight();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410");

	ImageBuffer current_img;
	AsciiOutput ascii_art;

	std::string current_file_path = "";
	char ramp_buffer[128] = " .:-=+*#%@";

	// Grid controls
	int target_columns = 100;
	int target_rows = 50;
	bool lock_aspect = true;

	// Tuning
	float brightness = 0.0f;
	float contrast = 1.0f;
	bool invert_image = false;
	float base_opacity = 0.4f;
	bool show_base_layer = true;
	bool show_ascii_layer = true;


	ImVec4 text_color = ImVec4(0.0f, 1.0f, 0.4f, 1.0f);
	ImVec4 bg_color = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);

	// Spacing & Glitch Controls
	float char_spacing_x = 7.0f;   // Horizontal kerning (px)
	float char_spacing_y = 12.0f;  // Vertical line spacing (px)
	float glitch_intensity = 0.0f; // Random jitter offset
	bool glitch_per_frame = false;

	bool export_full_canvas = false;				 // Toggle between full grid and viewport-only export
	ImVec2 current_viewport_size = ImVec2(0, 0); // Stores live Viewport dimensions

	bool pending_export = false;
	std::string export_file_path = "";

	std::mt19937 rng(1337);

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

		// Sidebar
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(360, io.DisplaySize.y), ImGuiCond_Always);
		ImGui::Begin("Glitch & Matrix Controls", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		ImGui::Text("1. Reference Image");
		if (ImGui::Button("Open File (Native Dialog)...", ImVec2(-1, 30)))
		{
			std::string selected_path = OpenNativeImageDialog();
			if (!selected_path.empty())
			{
				current_file_path = selected_path;
				int w, h, ch;
				unsigned char *data = stbi_load(current_file_path.c_str(), &w, &h, &ch, 4);
				if (data)
				{
					if (current_img.pixels) stbi_image_free(current_img.pixels);
					if (current_img.textureID) glDeleteTextures(1, &current_img.textureID);

					current_img.pixels = data;
					current_img.width = w;
					current_img.height = h;
					current_img.channels = 4;
					current_img.textureID = create_texture_from_pixels(data, w, h, 4);

					target_columns = 80;
					float aspect = static_cast<float>(h) / static_cast<float>(w);
					target_rows = std::max(1, static_cast<int>(target_columns * aspect * 0.55f));

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
		ImGui::Checkbox("Lock Aspect Ratio", &lock_aspect);
		bool grid_changed = false;
		if (ImGui::SliderInt("Columns (X)", &target_columns, 10, 500))
			grid_changed = true;

		if (lock_aspect && grid_changed && current_img.height > 0)
		{
			float aspect = static_cast<float>(current_img.height) / static_cast<float>(current_img.width);
			target_rows = std::max(1, static_cast<int>(target_columns * aspect * 0.55f));
		}
		else
		{
			if (ImGui::SliderInt("Rows (Y)", &target_rows, 10, 500))
				grid_changed = true;
		}
		if (grid_changed && current_img.pixels)
		{
			ascii_art = generate_ascii(current_img, target_columns, target_rows, brightness, contrast, invert_image,ramp_buffer);
		}
		ImGui::Text("Grid Resolution: %d x %d", ascii_art.cols, ascii_art.rows);

		ImGui::Separator();
		ImGui::Text("3. Image Inversion & Processing");
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
		ImGui::Text("4. Character Spacing & Glitch Mechanics");
		ImGui::SliderFloat("Horizontal Spacing", &char_spacing_x, 1.0f, 30.0f);
		ImGui::SliderFloat("Vertical Spacing", &char_spacing_y, 1.0f, 30.0f);
		ImGui::SliderFloat("Glitch Jitter", &glitch_intensity, 0.0f, 50.0f);
		ImGui::Checkbox("Animate Glitch", &glitch_per_frame);

		ImGui::Separator();
		ImGui::Text("5. Color & Layer Opacity");
		ImGui::ColorEdit4("Canvas Background", (float *)&bg_color);
		ImGui::ColorEdit4("Text Color", (float *)&text_color);
		ImGui::Checkbox("Show Base Image", &show_base_layer);
		ImGui::SliderFloat("Base Layer Opacity", &base_opacity, 0.0f, 1.0f);
		ImGui::Checkbox("Show ASCII Overlay", &show_ascii_layer);

		ImGui::Separator();
		ImGui::Text("6. Exports");
		ImGui::Checkbox("Export Entire ASCII Canvas", &export_full_canvas);
		if (ImGui::Button("Export Image (.PNG / .JPG)", ImVec2(-1, 30)))
		{
			std::string save_path = SaveNativeFileDialog("ascii_art.png");
			if (!save_path.empty())
			{
				export_file_path = save_path;
				pending_export = true; // Schedule export for end-of-frame to prevent frame state assertions
			}
		}
		ImGui::End();

		// Viewport
		ImGui::SetNextWindowPos(ImVec2(360, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 360, io.DisplaySize.y), ImGuiCond_Always);
		ImGui::Begin("Canvas Viewport", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar);

		current_viewport_size = ImGui::GetContentRegionAvail();
		ImDrawList *draw_list = ImGui::GetWindowDrawList();
		ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

		if (current_img.textureID > 0)
		{
			float canvas_w = ascii_art.cols * char_spacing_x;
			float canvas_h = ascii_art.rows * char_spacing_y;

			// 1. Draw Base Reference Layer (Stretched to match custom output grid)
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

			// 2. Custom Character-by-Character Glitch & Spacing Rendering Pipeline
			// to be reviewed really
			if (show_ascii_layer && !ascii_art.text.empty())
			{
				ImU32 txt_col = ImGui::ColorConvertFloat4ToU32(text_color);
				ImFont *font = ImGui::GetFont();
				float font_size = ImGui::GetFontSize();

				if (!glitch_per_frame)
				{
					rng.seed(1337);
				}
				std::uniform_real_distribution<float> jitter_dist(-glitch_intensity, glitch_intensity);

				int col = 0;
				int row = 0;

				for (char ch : ascii_art.text)
				{
					if (ch == '\n')
					{
						row++;
						col = 0;
						continue;
					}

					// Apply character spacing + random glitch displacement
					float offset_x = (glitch_intensity > 0.0f) ? jitter_dist(rng) : 0.0f;
					float offset_y = (glitch_intensity > 0.0f) ? jitter_dist(rng) : 0.0f;

					ImVec2 char_pos(
						canvas_pos.x + (col * char_spacing_x) + offset_x,
						canvas_pos.y + (row * char_spacing_y) + offset_y);

					char buf[2] = {ch, '\0'};
					draw_list->AddText(font, font_size, char_pos, txt_col, buf);

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

		// Render Frame
		ImGui::Render();

		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Execute deferred image export safely after ImGui render pass completes
		if (pending_export)
		{
			export_ascii_fbo_gpu(
				export_file_path,
				ascii_art,
				char_spacing_x,
				char_spacing_y,
				bg_color,
				text_color,
				glitch_intensity,
				export_full_canvas,
				current_viewport_size);
			pending_export = false;
		}

		glfwSwapBuffers(window);
	}

	if (current_img.pixels)
		stbi_image_free(current_img.pixels);
	if (current_img.textureID)
		glDeleteTextures(1, &current_img.textureID);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
