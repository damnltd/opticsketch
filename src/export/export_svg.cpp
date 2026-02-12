#include "export/export_svg.h"
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

static std::string colorToSvg(const glm::vec3& c) {
    int r = static_cast<int>(std::round(c.x * 255.0f));
    int g = static_cast<int>(std::round(c.y * 255.0f));
    int b = static_cast<int>(std::round(c.z * 255.0f));
    std::ostringstream oss;
    oss << "rgb(" << r << "," << g << "," << b << ")";
    return oss.str();
}

static std::string colorToSvgFill(const glm::vec3& c, float opacity = 0.2f) {
    int r = static_cast<int>(std::round(c.x * 255.0f));
    int g = static_cast<int>(std::round(c.y * 255.0f));
    int b = static_cast<int>(std::round(c.z * 255.0f));
    std::ostringstream oss;
    oss << "rgba(" << r << "," << g << "," << b << "," << opacity << ")";
    return oss.str();
}

static std::string fmt(float v, int prec = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}

static std::string escapeXml(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&':  result += "&amp;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default:   result += c; break;
        }
    }
    return result;
}

bool exportSvg(const std::string& path, Scene* scene, SceneStyle* style,
               const SvgExportOptions& opts) {
    if (!scene) return false;

    std::ofstream out(path);
    if (!out.is_open()) return false;

    // Scale: 25mm grid -> 40px (so 1mm = 1.6px)
    const float scale = 1.6f;

    // Compute bounding box of all visible scene content
    float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;

    auto expandBounds = [&](float x, float y) {
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    };

    for (const auto& elem : scene->getElements()) {
        if (!elem->visible) continue;
        glm::vec3 wMin, wMax;
        elem->getWorldBounds(wMin, wMax);
        expandBounds(wMin.x * scale, -wMax.z * scale);
        expandBounds(wMax.x * scale, -wMin.z * scale);
    }
    for (const auto& beam : scene->getBeams()) {
        if (!beam->visible) continue;
        expandBounds(beam->start.x * scale, -beam->start.z * scale);
        expandBounds(beam->end.x * scale, -beam->end.z * scale);
    }
    for (const auto& ann : scene->getAnnotations()) {
        if (!ann->visible) continue;
        expandBounds(ann->position.x * scale, -ann->position.z * scale);
    }
    for (const auto& meas : scene->getMeasurements()) {
        if (!meas->visible) continue;
        expandBounds(meas->startPoint.x * scale, -meas->startPoint.z * scale);
        expandBounds(meas->endPoint.x * scale, -meas->endPoint.z * scale);
    }

    if (minX > maxX) { minX = 0; maxX = 100; minY = 0; maxY = 100; }

    float padding = 40.0f;
    float vbX = minX - padding;
    float vbY = minY - padding;
    float vbW = (maxX - minX) + 2.0f * padding;
    float vbH = (maxY - minY) + 2.0f * padding;

    // SVG header
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "width=\"" << fmt(vbW) << "\" height=\"" << fmt(vbH) << "\" "
        << "viewBox=\"" << fmt(vbX) << " " << fmt(vbY) << " " << fmt(vbW) << " " << fmt(vbH) << "\">\n";

    // Defs: arrowheads + hatching pattern
    out << "<defs>\n";
    out << "  <marker id=\"arrowhead\" markerWidth=\"10\" markerHeight=\"7\" "
        << "refX=\"10\" refY=\"3.5\" orient=\"auto\">\n";
    out << "    <polygon points=\"0 0, 10 3.5, 0 7\" fill=\"currentColor\" />\n";
    out << "  </marker>\n";
    out << "  <marker id=\"arrowhead-rev\" markerWidth=\"10\" markerHeight=\"7\" "
        << "refX=\"0\" refY=\"3.5\" orient=\"auto\">\n";
    out << "    <polygon points=\"10 0, 0 3.5, 10 7\" fill=\"currentColor\" />\n";
    out << "  </marker>\n";
    out << "</defs>\n\n";

    // --- Optical Axis ---
    if (opts.showOpticalAxis) {
        OpticalAxis axis = detectOpticalAxis(scene->getElements());
        if (axis.valid) {
            float ax1 = axis.start.x * scale;
            float ay1 = -axis.start.y * scale;
            float ax2 = axis.end.x * scale;
            float ay2 = -axis.end.y * scale;
            out << "<!-- Optical Axis -->\n";
            out << "<line x1=\"" << fmt(ax1) << "\" y1=\"" << fmt(ay1)
                << "\" x2=\"" << fmt(ax2) << "\" y2=\"" << fmt(ay2)
                << "\" stroke=\"gray\" stroke-width=\"0.8\" stroke-dasharray=\"6,4\" />\n\n";
        }
    }

    // --- Elements ---
    out << "<!-- Elements -->\n";
    out << "<g id=\"elements\">\n";
    for (const auto& elem : scene->getElements()) {
        if (!elem->visible) continue;

        // XZ projection: world X -> SVG X, world Z -> SVG Y (inverted)
        float cx = elem->transform.position.x * scale;
        float cy = -elem->transform.position.z * scale;

        // Y-axis rotation
        glm::vec3 euler = glm::eulerAngles(elem->transform.rotation);
        float rotDeg = glm::degrees(euler.y);

        // Size
        float w = (elem->boundsMax.x - elem->boundsMin.x) * elem->transform.scale.x * scale;
        float h = (elem->boundsMax.z - elem->boundsMin.z) * elem->transform.scale.z * scale;
        if (w < 4.0f) w = 16.0f;
        if (h < 4.0f) h = 16.0f;

        int colorIdx = static_cast<int>(elem->type);
        glm::vec3 color = (style && colorIdx < kElementTypeCount) ?
            style->elementColors[colorIdx] : glm::vec3(0.5f);
        std::string strokeColor = colorToSvg(color);
        std::string fillColor = colorToSvgFill(color, 0.2f);

        // Render using optical symbol
        OpticalSymbol sym = getOpticalSymbol(elem->type);
        out << renderSymbolSvg(sym, cx, cy, w, h, rotDeg, strokeColor, fillColor);

        // Label
        if (elem->showLabel) {
            out << "  <text x=\"" << fmt(cx) << "\" y=\"" << fmt(cy + h / 2 + 12)
                << "\" text-anchor=\"middle\" font-size=\"10\" font-family=\"serif\" fill=\""
                << strokeColor << "\">"
                << escapeXml(elem->label) << "</text>\n";
        }
    }
    out << "</g>\n\n";

    // --- Beams ---
    out << "<!-- Beams -->\n";
    out << "<g id=\"beams\">\n";
    for (const auto& beam : scene->getBeams()) {
        if (!beam->visible) continue;

        float sx = beam->start.x * scale;
        float sy = -beam->start.z * scale;
        float ex = beam->end.x * scale;
        float ey = -beam->end.z * scale;
        std::string beamColor = colorToSvg(beam->color);

        if (beam->isGaussian) {
            float beamLen = beam->getLength();
            if (beamLen > 1e-6f) {
                glm::vec3 dir = beam->getDirection();
                glm::vec3 perp(-dir.z, 0.0f, dir.x);

                const int nSamples = 32;
                std::vector<std::pair<float, float>> upper, lower;
                for (int i = 0; i <= nSamples; ++i) {
                    float t = static_cast<float>(i) / nSamples;
                    float dist = t * beamLen;
                    float zFromWaist = dist - beam->waistPosition * beamLen;
                    float zM = std::abs(zFromWaist) * 0.001f;
                    float radiusMM = beam->beamRadiusAt(zM) * 1000.0f;

                    glm::vec3 center = beam->start + dir * dist;
                    glm::vec3 up = center + perp * radiusMM;
                    glm::vec3 dn = center - perp * radiusMM;

                    upper.push_back({up.x * scale, -up.z * scale});
                    lower.push_back({dn.x * scale, -dn.z * scale});
                }

                out << "  <path d=\"M " << fmt(upper[0].first) << " " << fmt(upper[0].second);
                for (size_t i = 1; i < upper.size(); ++i) {
                    out << " L " << fmt(upper[i].first) << " " << fmt(upper[i].second);
                }
                for (int i = static_cast<int>(lower.size()) - 1; i >= 0; --i) {
                    out << " L " << fmt(lower[i].first) << " " << fmt(lower[i].second);
                }
                out << " Z\" fill=\"" << beamColor << "\" fill-opacity=\"0.15\" stroke=\"none\" />\n";
            }
        }

        // Centerline with arrowhead
        out << "  <line x1=\"" << fmt(sx) << "\" y1=\"" << fmt(sy)
            << "\" x2=\"" << fmt(ex) << "\" y2=\"" << fmt(ey)
            << "\" stroke=\"" << beamColor << "\" stroke-width=\"2\" "
            << "marker-end=\"url(#arrowhead)\" />\n";
    }
    out << "</g>\n\n";

    // --- Annotations ---
    out << "<!-- Annotations -->\n";
    out << "<g id=\"annotations\">\n";
    for (const auto& ann : scene->getAnnotations()) {
        if (!ann->visible) continue;

        float ax = ann->position.x * scale;
        float ay = -ann->position.z * scale;

        out << "  <text x=\"" << fmt(ax) << "\" y=\"" << fmt(ay)
            << "\" font-size=\"" << fmt(ann->fontSize, 0) << "\" font-family=\"serif\" fill=\""
            << colorToSvg(ann->color) << "\">"
            << escapeXml(ann->text) << "</text>\n";
    }
    out << "</g>\n\n";

    // --- Measurements ---
    out << "<!-- Measurements -->\n";
    out << "<g id=\"measurements\">\n";
    for (const auto& meas : scene->getMeasurements()) {
        if (!meas->visible) continue;

        float sx = meas->startPoint.x * scale;
        float sy = -meas->startPoint.z * scale;
        float ex = meas->endPoint.x * scale;
        float ey = -meas->endPoint.z * scale;
        std::string measColor = colorToSvg(meas->color);

        std::ostringstream distStr;
        distStr << std::fixed << std::setprecision(1) << meas->getDistance() << " mm";

        // Dimension line with arrowheads at both ends
        out << "  <line x1=\"" << fmt(sx) << "\" y1=\"" << fmt(sy)
            << "\" x2=\"" << fmt(ex) << "\" y2=\"" << fmt(ey)
            << "\" stroke=\"" << measColor << "\" stroke-width=\"1\" "
            << "marker-start=\"url(#arrowhead-rev)\" marker-end=\"url(#arrowhead)\" />\n";

        // Distance label at midpoint
        float mx = (sx + ex) / 2.0f;
        float my = (sy + ey) / 2.0f;
        out << "  <text x=\"" << fmt(mx) << "\" y=\"" << fmt(my - 4)
            << "\" text-anchor=\"middle\" font-size=\"" << fmt(meas->fontSize, 0)
            << "\" font-family=\"serif\" fill=\"" << measColor << "\">"
            << escapeXml(distStr.str()) << "</text>\n";
    }
    out << "</g>\n\n";

    // --- Scale Bar ---
    if (opts.showScaleBar) {
        float sceneExtent = std::max(maxX - minX, maxY - minY) / scale;
        ScaleBar bar = chooseScaleBar(sceneExtent);
        // Position at bottom-right
        float sbX = maxX - bar.lengthMm * scale;
        float sbY = maxY + 20.0f;
        out << renderScaleBarSvg(bar, scale, sbX, sbY);
    }

    out << "</svg>\n";

    return out.good();
}

} // namespace opticsketch
