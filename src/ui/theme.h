#pragma once

#include <imgui.h>

namespace opticsketch {

/**
 * Setup Moonlight theme for ImGui
 * Based on Moonlight theme by deathsu/madam-herta, modified for OpticSketch
 * Original: https://github.com/Madam-Herta/Moonlight
 */
void SetupMoonlightTheme();

/**
 * Setup fonts for ImGui
 * Attempts to load Inter font, falls back to default if not available
 */
void SetupFonts();

} // namespace opticsketch
