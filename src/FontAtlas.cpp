#include "AsciiArt.h"

#include <algorithm>
#include <iostream>

AsciiArt::FontSet AsciiArt::rebuild_font_atlas(const BuiltinFont &ascii_font_definition, float ascii_font_size)
{
	ImGuiIO &io = ImGui::GetIO();
	ascii_font_size = std::clamp(
		ascii_font_size,
		Config::minimum_ascii_font_size,
		Config::maximum_ascii_font_size);

	io.Fonts->Clear();
	FontSet fonts;

	ImFontConfig interface_config;
	interface_config.OversampleH = 2;
	interface_config.OversampleV = 2;
	interface_config.PixelSnapH = false;

	fonts.interface_font = io.Fonts->AddFontFromFileTTF(
		interface_font_path,
		interface_font_size,
		&interface_config,
		interface_glyph_ranges);

	if (!fonts.interface_font)
	{
		std::cerr << "Failed to load the interface font\n";
		ImFontConfig fallback_config;
		fallback_config.SizePixels = interface_font_size;
		fonts.interface_font = io.Fonts->AddFontDefault(&fallback_config);
	}

	ImFontConfig ascii_config;
	ascii_config.OversampleH = 2;
	ascii_config.OversampleV = 2;
	ascii_config.PixelSnapH = false;

	fonts.ascii_font = io.Fonts->AddFontFromFileTTF(
		ascii_font_definition.path,
		ascii_font_size,
		&ascii_config,
		ascii_glyph_ranges);

	if (!fonts.ascii_font)
	{
		std::cerr << "Failed to load ASCII font: " << ascii_font_definition.path << '\n';
		fonts.ascii_font = fonts.interface_font;
	}

	io.FontDefault = fonts.interface_font;
	return fonts;
}
