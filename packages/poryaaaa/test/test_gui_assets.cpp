#include "test_assert.h"
#include "m4a_gui_font_assets.h"

#include <filesystem>

#ifndef PORYAAAA_FONT_DIR
#error "PORYAAAA_FONT_DIR must point at the poryaaaa CLAP GUI font assets"
#endif

extern "C" void test_gui_assets_run_all(void)
{
	const std::filesystem::path fontDir = PORYAAAA_FONT_DIR;
	ASSERT(std::filesystem::exists(fontDir / "Calamity-Regular.ttf"),
	       "Calamity-Regular.ttf is available for the poryaaaa CLAP GUI");
	ASSERT(std::filesystem::path(m4a_gui_regular_font_path()).filename() == "Calamity-Regular.ttf",
	       "poryaaaa CLAP GUI uses Calamity-Regular.ttf as the regular font");
	ASSERT(std::filesystem::exists(m4a_gui_bold_font_path()),
	       "poryaaaa CLAP GUI uses an available bold font");
	ASSERT(std::filesystem::path(m4a_gui_bold_font_path()).filename() == "Calamity-Bold.ttf",
	       "poryaaaa CLAP GUI uses Calamity-Bold.ttf as the bold font");
}
