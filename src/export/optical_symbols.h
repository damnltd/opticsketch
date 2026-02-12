#pragma once

#include "elements/element.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>

namespace opticsketch {

// 2D point in local symbol space. Origin = element center.
// X = along optical axis (right), Y = perpendicular (up).
struct SymPt { float x, y; };

// Path commands for 2D symbol (simplified SVG-like path)
enum class PathCmd { MoveTo, LineTo, ArcTo, Close };

struct PathSegment {
    PathCmd cmd;
    SymPt p;           // endpoint
    float radius = 0;  // arc radius (for ArcTo)
    bool largeArc = false;
    bool sweep = false;
};

// A drawable path with style
struct SymbolPath {
    std::vector<PathSegment> segments;
    bool filled = false;
    float fillOpacity = 0.2f;
    bool stroked = true;
    float strokeWidth = 1.5f;
    bool isHatching = false;
    bool isDashed = false;
};

// Complete symbol definition for one element type
struct OpticalSymbol {
    std::vector<SymbolPath> paths;
    float nominalWidth = 1.0f;
    float nominalHeight = 1.0f;
};

// Get the 2D optical symbol for an element type.
// Symbol is in local coordinates centered at (0,0).
OpticalSymbol getOpticalSymbol(ElementType type);

// Render symbol as SVG markup.
std::string renderSymbolSvg(const OpticalSymbol& sym,
                            float cx, float cy, float w, float h,
                            float rotDeg,
                            const std::string& strokeColor,
                            const std::string& fillColor);

// Render symbol as TikZ draw commands.
std::string renderSymbolTikz(const OpticalSymbol& sym,
                             float tx, float ty, float w, float h,
                             float rotDeg,
                             const std::string& colorName);

// --- Optical Axis Detection ---

struct OpticalAxis {
    glm::vec2 start;   // 2D projected start (x, z)
    glm::vec2 end;     // 2D projected end (x, z)
    bool valid = false;
};

// Detect dominant optical axis from element positions (XZ plane projection).
OpticalAxis detectOpticalAxis(const std::vector<std::unique_ptr<Element>>& elements);

// --- Scale Bar ---

struct ScaleBar {
    float lengthMm;
    std::string labelText;
};

// Choose appropriate scale bar length for the scene extent.
ScaleBar chooseScaleBar(float sceneExtentMm);

// Render scale bar as SVG at given position.
std::string renderScaleBarSvg(const ScaleBar& bar, float scale,
                               float x, float y);

// Render scale bar as TikZ at given position.
std::string renderScaleBarTikz(const ScaleBar& bar, float scale,
                                float x, float y);

} // namespace opticsketch
