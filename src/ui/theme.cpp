#include "ui/theme.h"
#include <imgui.h>
#include <cstdio>
#include <sys/stat.h>

namespace opticsketch {

void SetupMoonlightTheme() {
    // Moonlight style by deathsu/madam-herta (modified for OpticSketch)
    // Original: https://github.com/Madam-Herta/Moonlight
    
    ImGuiStyle& style = ImGui::GetStyle();
    
    // === GEOMETRY (slightly tighter for professional look) ===
    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.6f;
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.WindowRounding = 6.0f;           // Less round = more professional
    style.WindowBorderSize = 1.0f;
    style.WindowMinSize = ImVec2(200.0f, 100.0f);
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Right;
    style.ChildRounding = 4.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 4.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.FrameRounding = 4.0f;            // Subtle rounding
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.CellPadding = ImVec2(8.0f, 4.0f);
    style.IndentSpacing = 20.0f;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 12.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabMinSize = 8.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.TabBorderSize = 0.0f;
    // style.TabMinWidthForCloseButton = 0.0f;  // Not available in this ImGui version
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
    
    // Anti-aliasing settings for better text rendering
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
    style.AntiAliasedLinesUseTex = true; // Use texture-based anti-aliasing
    
    // === COLORS (Moonlight base with subtle accent adjustments) ===
    ImVec4* colors = style.Colors;
    
    // Backgrounds - deep blue-grey
    colors[ImGuiCol_WindowBg]             = ImVec4(0.078f, 0.086f, 0.102f, 1.0f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.092f, 0.100f, 0.116f, 1.0f);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.078f, 0.086f, 0.102f, 0.98f);
    
    // Text
    colors[ImGuiCol_Text]                 = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
    
    // Borders
    colors[ImGuiCol_Border]               = ImVec4(0.20f, 0.22f, 0.27f, 1.0f);
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Frame backgrounds
    colors[ImGuiCol_FrameBg]              = ImVec4(0.112f, 0.126f, 0.154f, 1.0f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.157f, 0.169f, 0.192f, 1.0f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.157f, 0.169f, 0.192f, 1.0f);
    
    // Title bar
    colors[ImGuiCol_TitleBg]              = ImVec4(0.047f, 0.055f, 0.071f, 1.0f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.047f, 0.055f, 0.071f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.078f, 0.086f, 0.102f, 1.0f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.098f, 0.106f, 0.122f, 1.0f);
    
    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.047f, 0.055f, 0.071f, 1.0f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.118f, 0.133f, 0.149f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.157f, 0.169f, 0.192f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.200f, 0.220f, 0.270f, 1.0f);
    
    // Accent color: Soft scientific blue (not aggressive yellow)
    // Changed from Moonlight's yellow to a calm blue for professional look
    const ImVec4 accent       = ImVec4(0.40f, 0.60f, 0.90f, 1.0f);  // Soft blue
    const ImVec4 accentHover  = ImVec4(0.50f, 0.70f, 1.00f, 1.0f);
    const ImVec4 accentActive = ImVec4(0.35f, 0.55f, 0.85f, 1.0f);
    
    // Interactive elements with accent
    colors[ImGuiCol_CheckMark]            = accent;
    colors[ImGuiCol_SliderGrab]           = accent;
    colors[ImGuiCol_SliderGrabActive]     = accentHover;
    
    // Buttons
    colors[ImGuiCol_Button]               = ImVec4(0.118f, 0.133f, 0.149f, 1.0f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.182f, 0.190f, 0.197f, 1.0f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.155f, 0.155f, 0.155f, 1.0f);
    
    // Headers (used in collapsing headers, tree nodes)
    colors[ImGuiCol_Header]               = ImVec4(0.141f, 0.163f, 0.206f, 1.0f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.180f, 0.200f, 0.250f, 1.0f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.200f, 0.220f, 0.280f, 1.0f);
    
    // Separator
    colors[ImGuiCol_Separator]            = ImVec4(0.20f, 0.22f, 0.27f, 1.0f);
    colors[ImGuiCol_SeparatorHovered]     = accent;
    colors[ImGuiCol_SeparatorActive]      = accent;
    
    // Resize grip
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_ResizeGripHovered]    = accent;
    colors[ImGuiCol_ResizeGripActive]     = accentHover;
    
    // Tabs
    colors[ImGuiCol_Tab]                  = ImVec4(0.078f, 0.086f, 0.102f, 1.0f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.180f, 0.200f, 0.250f, 1.0f);
    colors[ImGuiCol_TabActive]            = ImVec4(0.141f, 0.163f, 0.206f, 1.0f);
    colors[ImGuiCol_TabUnfocused]         = ImVec4(0.078f, 0.086f, 0.102f, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.125f, 0.180f, 0.280f, 1.0f);
    
    // Docking
    colors[ImGuiCol_DockingPreview]       = accent;
    colors[ImGuiCol_DockingEmptyBg]       = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);
    
    // Plot
    colors[ImGuiCol_PlotLines]            = ImVec4(0.52f, 0.60f, 0.70f, 1.0f);
    colors[ImGuiCol_PlotLinesHovered]     = accent;
    colors[ImGuiCol_PlotHistogram]        = accent;
    colors[ImGuiCol_PlotHistogramHovered] = accentHover;
    
    // Tables
    colors[ImGuiCol_TableHeaderBg]        = ImVec4(0.047f, 0.055f, 0.071f, 1.0f);
    colors[ImGuiCol_TableBorderStrong]    = ImVec4(0.20f, 0.22f, 0.27f, 1.0f);
    colors[ImGuiCol_TableBorderLight]     = ImVec4(0.15f, 0.17f, 0.20f, 1.0f);
    colors[ImGuiCol_TableRowBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt]        = ImVec4(0.10f, 0.11f, 0.12f, 0.5f);
    
    // Selection & interaction
    colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.30f, 0.45f, 0.70f, 0.5f);
    colors[ImGuiCol_DragDropTarget]       = accent;
    colors[ImGuiCol_NavHighlight]         = accent;
    colors[ImGuiCol_NavWindowingHighlight]= accent;
    colors[ImGuiCol_NavWindowingDimBg]    = ImVec4(0.10f, 0.10f, 0.20f, 0.5f);
    colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.10f, 0.10f, 0.20f, 0.5f);
}

// Helper function to check if file exists
static bool FileExists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

void SetupFonts() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Primary font: Inter or Roboto (clean, readable)
    // - Inter: https://rsms.me/inter/
    // - Or use system fonts
    float fontSize = 16.0f;
    
    // Configure font rendering for better quality
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 3;  // Increased from 2 for better horizontal anti-aliasing
    fontConfig.OversampleV = 3;  // Increased from 2 for better vertical anti-aliasing
    fontConfig.PixelSnapH = true; // Snap to pixel boundaries for crisp rendering
    fontConfig.RasterizerMultiply = 1.2f; // Slightly brighter for better visibility
    
    // Try to load Inter, fallback to default
    ImFont* mainFont = nullptr;
    const char* interPath = "assets/fonts/Inter-Regular.ttf";
    
    // Check if file exists before trying to load
    if (FileExists(interPath)) {
        mainFont = io.Fonts->AddFontFromFileTTF(interPath, fontSize, &fontConfig);
    }
    
    if (!mainFont) {
        // Try system fonts for better rendering
        const char* systemFonts[] = {
            // Windows
            "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\calibri.ttf",
            "C:\\Windows\\Fonts\\arial.ttf",
            // Linux
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            nullptr
        };
        
        bool systemFontLoaded = false;
        for (int i = 0; systemFonts[i] != nullptr; ++i) {
            if (FileExists(systemFonts[i])) {
                mainFont = io.Fonts->AddFontFromFileTTF(systemFonts[i], fontSize, &fontConfig);
                if (mainFont) {
                    systemFontLoaded = true;
                    break;
                }
            }
        }
        
        // Final fallback: use ImGui's default with improved config
        if (!systemFontLoaded) {
            io.Fonts->AddFontDefault(&fontConfig);
        }
    }
    
    // Monospace font for values, code, part numbers
    // - JetBrains Mono or Source Code Pro
    ImFontConfig monoConfig;
    monoConfig.OversampleH = 3;
    monoConfig.OversampleV = 3;
    monoConfig.PixelSnapH = true;
    monoConfig.RasterizerMultiply = 1.2f;
    
    const char* monoFonts[] = {
        "assets/fonts/JetBrainsMono-Regular.ttf",
        // Windows
        "C:\\Windows\\Fonts\\consola.ttf",
        "C:\\Windows\\Fonts\\cascadiamono.ttf",
        // Linux
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        nullptr
    };
    for (int i = 0; monoFonts[i] != nullptr; ++i) {
        if (FileExists(monoFonts[i])) {
            if (io.Fonts->AddFontFromFileTTF(monoFonts[i], 15.0f, &monoConfig))
                break;
        }
    }
    
    // Build font atlas with better quality settings
    io.Fonts->Build();
    
    // Enable font texture filtering for smoother rendering
    // This is done automatically by ImGui, but we can ensure it's set correctly
}

} // namespace opticsketch
