#include "export/export_tikz.h"
#include "export/optical_symbols.h"
#include "scene/scene.h"
#include "elements/element.h"
#include "elements/annotation.h"
#include "elements/measurement.h"
#include "render/beam.h"
#include "style/scene_style.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cfloat>

namespace opticsketch {

// Convert glm::vec3 color [0,1] to LaTeX {rgb,255:r,g,b} format
static std::string colorToTikz(const glm::vec3& c) {
    int r = static_cast<int>(std::round(c.x * 255.0f));
    int g = static_cast<int>(std::round(c.y * 255.0f));
    int b = static_cast<int>(std::round(c.z * 255.0f));
    std::ostringstream oss;
    oss << "{rgb,255:red," << r << ";green," << g << ";blue," << b << "}";
    return oss.str();
}

// Define a named color in the preamble
static std::string defineColor(const std::string& name, const glm::vec3& c) {
    int r = static_cast<int>(std::round(c.x * 255.0f));
    int g = static_cast<int>(std::round(c.y * 255.0f));
    int b = static_cast<int>(std::round(c.z * 255.0f));
    std::ostringstream oss;
    oss << "\\definecolor{" << name << "}{RGB}{" << r << "," << g << "," << b << "}";
    return oss.str();
}

// Format a float with fixed precision
static std::string fmt(float v, int prec = 3) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}

// Escape special LaTeX characters in text
static std::string escapeLatex(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&':  result += "\\&"; break;
            case '%':  result += "\\%"; break;
            case '$':  result += "\\$"; break;
            case '#':  result += "\\#"; break;
            case '_':  result += "\\_"; break;
            case '{':  result += "\\{"; break;
            case '}':  result += "\\}"; break;
            case '~':  result += "\\textasciitilde{}"; break;
            case '^':  result += "\\textasciicircum{}"; break;
            case '\\': result += "\\textbackslash{}"; break;
            default:   result += c; break;
        }
    }
    return result;
}

bool exportTikz(const std::string& path, Scene* scene, SceneStyle* style,
                const TikzExportOptions& opts) {
    if (!scene) return false;

    std::ofstream out(path);
    if (!out.is_open()) return false;

    // Scale factor: world units (mm) to tikz cm
    const float scale = 1.0f / 25.0f; // 25mm grid spacing -> 1cm in tikz

    // Document preamble
    out << "\\documentclass[tikz,border=10pt]{standalone}\n";
    out << "\\usepackage{tikz}\n";
    out << "\\usetikzlibrary{shapes.geometric,arrows.meta,calc,patterns}\n";
    out << "\n";

    // Define colors
    out << "% Element colors\n";
    if (style) {
        for (int i = 0; i < kElementTypeCount; ++i) {
            out << defineColor("elemcolor" + std::to_string(i), style->elementColors[i]) << "\n";
        }
    }
    out << "\n";

    out << "\\begin{document}\n";
    out << "\\begin{tikzpicture}[\n";
    out << "  every node/.style={font=\\footnotesize},\n";
    out << "  >=Stealth\n";
    out << "]\n\n";

    // --- Optical Axis ---
    if (opts.showOpticalAxis) {
        OpticalAxis axis = detectOpticalAxis(scene->getElements());
        if (axis.valid) {
            float ax1 = axis.start.x * scale;
            float ay1 = axis.start.y * scale;
            float ax2 = axis.end.x * scale;
            float ay2 = axis.end.y * scale;
            out << "% Optical axis\n";
            out << "\\draw[gray, thin, dashed] ("
                << fmt(ax1, 3) << "," << fmt(ay1, 3) << ") -- ("
                << fmt(ax2, 3) << "," << fmt(ay2, 3) << ");\n\n";
        }
    }

    // --- Elements ---
    out << "% Elements\n";
    for (const auto& elem : scene->getElements()) {
        if (!elem->visible) continue;

        // XZ projection: world X -> tikz X, world Z -> tikz Y
        float tx = elem->transform.position.x * scale;
        float ty = elem->transform.position.z * scale;

        // Extract Y-axis rotation angle from quaternion
        glm::vec3 euler = glm::eulerAngles(elem->transform.rotation);
        float rotDeg = glm::degrees(euler.y);

        // Size from bounds and scale
        float w = (elem->boundsMax.x - elem->boundsMin.x) * elem->transform.scale.x * scale;
        float h = (elem->boundsMax.z - elem->boundsMin.z) * elem->transform.scale.z * scale;
        if (w < 0.1f) w = 0.4f;
        if (h < 0.1f) h = 0.4f;

        int colorIdx = static_cast<int>(elem->type);
        std::string colorName = (style && colorIdx < kElementTypeCount) ?
            "elemcolor" + std::to_string(colorIdx) : "black";

        // Render using optical symbol
        OpticalSymbol sym = getOpticalSymbol(elem->type);
        out << renderSymbolTikz(sym, tx, ty, w, h, rotDeg, colorName);

        // Label below the element
        if (elem->showLabel) {
            out << "\\node[below, " << colorName << "] at ("
                << fmt(tx, 3) << "," << fmt(ty - h / 2.0f - 0.15f, 3) << ") {"
                << escapeLatex(elem->label) << "};\n";
        }
    }
    out << "\n";

    // --- Beams ---
    out << "% Beams\n";
    for (const auto& beam : scene->getBeams()) {
        if (!beam->visible) continue;

        float sx = beam->start.x * scale;
        float sy = beam->start.z * scale;
        float ex = beam->end.x * scale;
        float ey = beam->end.z * scale;

        std::string beamColor = defineColor("beamcol_" + beam->id, beam->color);
        // Inline color definition
        out << "{ " << beamColor << "\n";

        if (beam->isGaussian) {
            // Draw Gaussian envelope as a filled path
            float beamLen = beam->getLength();
            if (beamLen > 1e-6f) {
                glm::vec3 dir = beam->getDirection();
                // Perpendicular in XZ plane
                glm::vec3 perp(-dir.z, 0.0f, dir.x);

                const int nSamples = 32;
                std::vector<std::pair<float, float>> upper, lower;
                for (int i = 0; i <= nSamples; ++i) {
                    float t = static_cast<float>(i) / nSamples;
                    float dist = t * beamLen;
                    float zFromWaist = dist - beam->waistPosition * beamLen;
                    float radius = beam->beamRadiusAt(std::abs(zFromWaist)) * 1000.0f; // m -> mm

                    glm::vec3 center = beam->start + dir * dist;
                    glm::vec3 up = center + perp * radius;
                    glm::vec3 dn = center - perp * radius;

                    upper.push_back({up.x * scale, up.z * scale});
                    lower.push_back({dn.x * scale, dn.z * scale});
                }

                out << "  \\fill[beamcol_" << beam->id << ", opacity=0.2] ";
                for (size_t i = 0; i < upper.size(); ++i) {
                    out << "(" << fmt(upper[i].first, 3) << "," << fmt(upper[i].second, 3) << ")";
                    if (i + 1 < upper.size()) out << " -- ";
                }
                out << " -- ";
                for (int i = static_cast<int>(lower.size()) - 1; i >= 0; --i) {
                    out << "(" << fmt(lower[i].first, 3) << "," << fmt(lower[i].second, 3) << ")";
                    if (i > 0) out << " -- ";
                }
                out << " -- cycle;\n";
            }
        }

        // Centerline
        out << "  \\draw[->, beamcol_" << beam->id << ", thick] ("
            << fmt(sx, 3) << "," << fmt(sy, 3) << ") -- ("
            << fmt(ex, 3) << "," << fmt(ey, 3) << ");\n";
        out << "}\n";
    }
    out << "\n";

    // --- Annotations ---
    out << "% Annotations\n";
    for (const auto& ann : scene->getAnnotations()) {
        if (!ann->visible) continue;

        float ax = ann->position.x * scale;
        float ay = ann->position.z * scale;

        out << "\\node[anchor=south west, text=" << colorToTikz(ann->color)
            << "] at (" << fmt(ax, 3) << "," << fmt(ay, 3) << ") {"
            << escapeLatex(ann->text) << "};\n";
    }
    out << "\n";

    // --- Measurements ---
    out << "% Measurements\n";
    for (const auto& meas : scene->getMeasurements()) {
        if (!meas->visible) continue;

        float sx = meas->startPoint.x * scale;
        float sy = meas->startPoint.z * scale;
        float ex = meas->endPoint.x * scale;
        float ey = meas->endPoint.z * scale;

        std::ostringstream distStr;
        distStr << std::fixed << std::setprecision(1) << meas->getDistance() << " mm";

        out << "\\draw[<->, " << colorToTikz(meas->color) << "] ("
            << fmt(sx, 3) << "," << fmt(sy, 3) << ") -- node[midway, above, sloped] {"
            << escapeLatex(distStr.str()) << "} ("
            << fmt(ex, 3) << "," << fmt(ey, 3) << ");\n";
    }
    out << "\n";

    // --- Scale Bar ---
    if (opts.showScaleBar) {
        // Compute scene extent from elements
        float minProj = FLT_MAX, maxProj = -FLT_MAX;
        for (const auto& elem : scene->getElements()) {
            if (!elem->visible) continue;
            float proj = elem->transform.position.x;
            if (proj < minProj) minProj = proj;
            if (proj > maxProj) maxProj = proj;
        }
        if (minProj < maxProj) {
            float sceneExtent = maxProj - minProj;
            ScaleBar bar = chooseScaleBar(sceneExtent);
            // Position below the scene
            float barX = maxProj * scale - bar.lengthMm * scale;
            float barY = -1.5f; // below origin
            // Find lowest element Z for positioning
            float minZ = 0;
            for (const auto& elem : scene->getElements()) {
                if (!elem->visible) continue;
                float z = elem->transform.position.z * scale - 0.5f;
                if (z < minZ) minZ = z;
            }
            barY = minZ - 0.8f;
            out << renderScaleBarTikz(bar, scale, barX, barY);
        }
    }

    out << "\\end{tikzpicture}\n";
    out << "\\end{document}\n";

    return out.good();
}

} // namespace opticsketch
