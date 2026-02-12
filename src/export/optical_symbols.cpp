#include "export/optical_symbols.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cfloat>
#include <algorithm>

namespace opticsketch {

// ============ Helpers ============

static std::string fmt(float v, int prec = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}

static SymbolPath makePath(bool stroked = true, float strokeW = 1.5f,
                            bool filled = false, float fillOp = 0.2f) {
    SymbolPath p;
    p.stroked = stroked;
    p.strokeWidth = strokeW;
    p.filled = filled;
    p.fillOpacity = fillOp;
    return p;
}

static void moveTo(SymbolPath& p, float x, float y) {
    p.segments.push_back({PathCmd::MoveTo, {x, y}});
}

static void lineTo(SymbolPath& p, float x, float y) {
    p.segments.push_back({PathCmd::LineTo, {x, y}});
}

static void arcTo(SymbolPath& p, float x, float y, float r, bool large, bool sweep) {
    PathSegment seg;
    seg.cmd = PathCmd::ArcTo;
    seg.p = {x, y};
    seg.radius = r;
    seg.largeArc = large;
    seg.sweep = sweep;
    p.segments.push_back(seg);
}

static void closePath(SymbolPath& p) {
    p.segments.push_back({PathCmd::Close, {0, 0}});
}

// ============ Symbol Generators ============

static OpticalSymbol makeLaserSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 1.1f;
    sym.nominalHeight = 0.6f;

    // Body rectangle
    SymbolPath body = makePath(true, 1.5f, true, 0.2f);
    moveTo(body, -0.5f, -0.3f);
    lineTo(body, 0.35f, -0.3f);
    lineTo(body, 0.35f, 0.3f);
    lineTo(body, -0.5f, 0.3f);
    closePath(body);
    sym.paths.push_back(body);

    // Arrow indicating beam direction
    SymbolPath arrow = makePath(true, 1.5f, false);
    moveTo(arrow, 0.35f, 0.12f);
    lineTo(arrow, 0.55f, 0.0f);
    lineTo(arrow, 0.35f, -0.12f);
    sym.paths.push_back(arrow);

    return sym;
}

static OpticalSymbol makeMirrorSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.2f;
    sym.nominalHeight = 1.0f;

    // Curved reflective surface (thick arc)
    float r = 2.5f; // large radius for slight curvature
    SymbolPath surface = makePath(true, 2.5f, false);
    moveTo(surface, 0.05f, -0.45f);
    arcTo(surface, 0.05f, 0.45f, r, false, true);
    sym.paths.push_back(surface);

    // Hatching on back side
    SymbolPath hatch = makePath(true, 0.8f, false);
    hatch.isHatching = true;
    float step = 0.1f;
    for (float y = -0.4f; y <= 0.41f; y += step) {
        moveTo(hatch, 0.05f, y);
        lineTo(hatch, -0.08f, y - 0.06f);
    }
    sym.paths.push_back(hatch);

    return sym;
}

static OpticalSymbol makeLensSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.4f;
    sym.nominalHeight = 1.0f;

    // Biconvex lens: two opposing arcs
    float r = 0.7f; // arc radius for curvature

    // Left arc (curves left)
    SymbolPath leftArc = makePath(true, 1.5f, false);
    moveTo(leftArc, 0.0f, -0.45f);
    arcTo(leftArc, 0.0f, 0.45f, r, false, false);
    sym.paths.push_back(leftArc);

    // Right arc (curves right)
    SymbolPath rightArc = makePath(true, 1.5f, false);
    moveTo(rightArc, 0.0f, 0.45f);
    arcTo(rightArc, 0.0f, -0.45f, r, false, false);
    sym.paths.push_back(rightArc);

    return sym;
}

static OpticalSymbol makeBeamSplitterSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.8f;
    sym.nominalHeight = 0.8f;

    // Square outline
    SymbolPath square = makePath(true, 1.5f, true, 0.15f);
    moveTo(square, -0.4f, -0.4f);
    lineTo(square, 0.4f, -0.4f);
    lineTo(square, 0.4f, 0.4f);
    lineTo(square, -0.4f, 0.4f);
    closePath(square);
    sym.paths.push_back(square);

    // Diagonal line (splitting surface)
    SymbolPath diag = makePath(true, 1.0f, false);
    moveTo(diag, -0.4f, -0.4f);
    lineTo(diag, 0.4f, 0.4f);
    sym.paths.push_back(diag);

    return sym;
}

static OpticalSymbol makeDetectorSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.7f;
    sym.nominalHeight = 0.7f;

    // Rectangle body (darker fill)
    SymbolPath body = makePath(true, 1.5f, true, 0.4f);
    moveTo(body, -0.35f, -0.35f);
    lineTo(body, 0.35f, -0.35f);
    lineTo(body, 0.35f, 0.35f);
    lineTo(body, -0.35f, 0.35f);
    closePath(body);
    sym.paths.push_back(body);

    // Zigzag sensor pattern on left (input) face
    SymbolPath zigzag = makePath(true, 1.2f, false);
    moveTo(zigzag, -0.35f, -0.25f);
    lineTo(zigzag, -0.25f, -0.15f);
    lineTo(zigzag, -0.35f, -0.05f);
    lineTo(zigzag, -0.25f, 0.05f);
    lineTo(zigzag, -0.35f, 0.15f);
    lineTo(zigzag, -0.25f, 0.25f);
    sym.paths.push_back(zigzag);

    return sym;
}

static OpticalSymbol makeFilterSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.12f;
    sym.nominalHeight = 0.9f;

    // Thin rectangle outline
    SymbolPath rect = makePath(true, 1.5f, true, 0.15f);
    moveTo(rect, -0.04f, -0.45f);
    lineTo(rect, 0.04f, -0.45f);
    lineTo(rect, 0.04f, 0.45f);
    lineTo(rect, -0.04f, 0.45f);
    closePath(rect);
    sym.paths.push_back(rect);

    // Cross-hatching inside
    SymbolPath hatch = makePath(true, 0.6f, false);
    hatch.isHatching = true;
    float step = 0.08f;
    for (float y = -0.4f; y <= 0.41f; y += step) {
        moveTo(hatch, -0.04f, y);
        lineTo(hatch, 0.04f, y + 0.04f);
    }
    for (float y = -0.4f; y <= 0.41f; y += step) {
        moveTo(hatch, -0.04f, y);
        lineTo(hatch, 0.04f, y - 0.04f);
    }
    sym.paths.push_back(hatch);

    return sym;
}

static OpticalSymbol makeApertureSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.1f;
    sym.nominalHeight = 1.0f;

    // Upper blade
    SymbolPath upper = makePath(true, 3.0f, false);
    moveTo(upper, 0.0f, 0.15f);
    lineTo(upper, 0.0f, 0.5f);
    sym.paths.push_back(upper);

    // Lower blade
    SymbolPath lower = makePath(true, 3.0f, false);
    moveTo(lower, 0.0f, -0.15f);
    lineTo(lower, 0.0f, -0.5f);
    sym.paths.push_back(lower);

    return sym;
}

static OpticalSymbol makePrismSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.8f;
    sym.nominalHeight = 0.7f;

    SymbolPath tri = makePath(true, 1.5f, true, 0.15f);
    moveTo(tri, 0.0f, 0.35f);
    lineTo(tri, 0.4f, -0.35f);
    lineTo(tri, -0.4f, -0.35f);
    closePath(tri);
    sym.paths.push_back(tri);

    return sym;
}

static OpticalSymbol makePrismRASymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.8f;
    sym.nominalHeight = 0.8f;

    // Right-angle triangle
    SymbolPath tri = makePath(true, 1.5f, true, 0.15f);
    moveTo(tri, -0.4f, -0.4f);
    lineTo(tri, 0.4f, -0.4f);
    lineTo(tri, -0.4f, 0.4f);
    closePath(tri);
    sym.paths.push_back(tri);

    // Right-angle mark
    float m = 0.08f;
    SymbolPath mark = makePath(true, 1.0f, false);
    moveTo(mark, -0.4f + m, -0.4f);
    lineTo(mark, -0.4f + m, -0.4f + m);
    lineTo(mark, -0.4f, -0.4f + m);
    sym.paths.push_back(mark);

    return sym;
}

static OpticalSymbol makeGratingSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.3f;
    sym.nominalHeight = 1.0f;

    // Main vertical line
    SymbolPath mainLine = makePath(true, 2.0f, false);
    moveTo(mainLine, 0.0f, -0.45f);
    lineTo(mainLine, 0.0f, 0.45f);
    sym.paths.push_back(mainLine);

    // Angled tick marks
    SymbolPath ticks = makePath(true, 1.0f, false);
    float step = 0.1f;
    for (float y = -0.4f; y <= 0.41f; y += step) {
        moveTo(ticks, 0.0f, y);
        lineTo(ticks, 0.12f, y - 0.05f);
    }
    sym.paths.push_back(ticks);

    return sym;
}

static OpticalSymbol makeFiberCouplerSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.7f;
    sym.nominalHeight = 0.3f;

    // Small filled circle (coupler body)
    SymbolPath circle = makePath(true, 1.5f, true, 0.3f);
    // Approximate circle with arcs
    moveTo(circle, 0.12f, 0.0f);
    arcTo(circle, -0.12f, 0.0f, 0.12f, false, true);
    arcTo(circle, 0.12f, 0.0f, 0.12f, false, true);
    sym.paths.push_back(circle);

    // Fiber line extending left
    SymbolPath fiber = makePath(true, 1.5f, false);
    moveTo(fiber, -0.5f, 0.0f);
    lineTo(fiber, -0.12f, 0.0f);
    sym.paths.push_back(fiber);

    return sym;
}

static OpticalSymbol makeScreenSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.06f;
    sym.nominalHeight = 1.0f;

    // Thick vertical line
    SymbolPath line = makePath(true, 3.0f, false);
    moveTo(line, 0.0f, -0.5f);
    lineTo(line, 0.0f, 0.5f);
    sym.paths.push_back(line);

    return sym;
}

static OpticalSymbol makeMountSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.5f;
    sym.nominalHeight = 0.7f;

    // Vertical post
    SymbolPath post = makePath(true, 2.0f, false);
    moveTo(post, 0.0f, -0.1f);
    lineTo(post, 0.0f, 0.35f);
    sym.paths.push_back(post);

    // Horizontal base
    SymbolPath base = makePath(true, 2.5f, false);
    moveTo(base, -0.25f, -0.1f);
    lineTo(base, 0.25f, -0.1f);
    sym.paths.push_back(base);

    return sym;
}

static OpticalSymbol makeImportedMeshSymbol() {
    OpticalSymbol sym;
    sym.nominalWidth = 0.8f;
    sym.nominalHeight = 0.6f;

    // Dashed rectangle
    SymbolPath rect = makePath(true, 1.5f, false);
    rect.isDashed = true;
    moveTo(rect, -0.4f, -0.3f);
    lineTo(rect, 0.4f, -0.3f);
    lineTo(rect, 0.4f, 0.3f);
    lineTo(rect, -0.4f, 0.3f);
    closePath(rect);
    sym.paths.push_back(rect);

    return sym;
}

// ============ Public API ============

OpticalSymbol getOpticalSymbol(ElementType type) {
    switch (type) {
        case ElementType::Laser:        return makeLaserSymbol();
        case ElementType::Mirror:       return makeMirrorSymbol();
        case ElementType::Lens:         return makeLensSymbol();
        case ElementType::BeamSplitter: return makeBeamSplitterSymbol();
        case ElementType::Detector:     return makeDetectorSymbol();
        case ElementType::Filter:       return makeFilterSymbol();
        case ElementType::Aperture:     return makeApertureSymbol();
        case ElementType::Prism:        return makePrismSymbol();
        case ElementType::PrismRA:      return makePrismRASymbol();
        case ElementType::Grating:      return makeGratingSymbol();
        case ElementType::FiberCoupler: return makeFiberCouplerSymbol();
        case ElementType::Screen:       return makeScreenSymbol();
        case ElementType::Mount:        return makeMountSymbol();
        case ElementType::ImportedMesh: return makeImportedMeshSymbol();
        default:                        return makeImportedMeshSymbol();
    }
}

// ============ SVG Renderer ============

std::string renderSymbolSvg(const OpticalSymbol& sym,
                            float cx, float cy, float w, float h,
                            float rotDeg,
                            const std::string& strokeColor,
                            const std::string& fillColor) {
    std::ostringstream out;

    float sx = (sym.nominalWidth > 0.001f) ? w / sym.nominalWidth : 1.0f;
    float sy = (sym.nominalHeight > 0.001f) ? h / sym.nominalHeight : 1.0f;

    out << "<g transform=\"translate(" << fmt(cx) << "," << fmt(cy) << ")";
    if (std::abs(rotDeg) > 0.1f) {
        out << " rotate(" << fmt(-rotDeg, 1) << ")";
    }
    out << "\">\n";

    for (const auto& path : sym.paths) {
        out << "  <path d=\"";
        for (const auto& seg : path.segments) {
            float px = seg.p.x * sx;
            float py = seg.p.y * sy;
            switch (seg.cmd) {
                case PathCmd::MoveTo:
                    out << "M " << fmt(px) << " " << fmt(py) << " ";
                    break;
                case PathCmd::LineTo:
                    out << "L " << fmt(px) << " " << fmt(py) << " ";
                    break;
                case PathCmd::ArcTo: {
                    float rx = seg.radius * sx;
                    float ry = seg.radius * sy;
                    out << "A " << fmt(rx) << " " << fmt(ry) << " 0 "
                        << (seg.largeArc ? "1" : "0") << " "
                        << (seg.sweep ? "1" : "0") << " "
                        << fmt(px) << " " << fmt(py) << " ";
                    break;
                }
                case PathCmd::Close:
                    out << "Z ";
                    break;
            }
        }
        out << "\"";

        // Style
        if (path.filled) {
            out << " fill=\"" << fillColor << "\"";
            out << " fill-opacity=\"" << fmt(path.fillOpacity) << "\"";
        } else {
            out << " fill=\"none\"";
        }

        if (path.stroked) {
            out << " stroke=\"" << strokeColor << "\"";
            out << " stroke-width=\"" << fmt(path.strokeWidth) << "\"";
            if (path.isDashed) {
                out << " stroke-dasharray=\"4,3\"";
            }
        } else {
            out << " stroke=\"none\"";
        }

        out << " />\n";
    }

    out << "</g>\n";
    return out.str();
}

// ============ TikZ Renderer ============

std::string renderSymbolTikz(const OpticalSymbol& sym,
                             float tx, float ty, float w, float h,
                             float rotDeg,
                             const std::string& colorName) {
    std::ostringstream out;

    float sx = (sym.nominalWidth > 0.001f) ? w / sym.nominalWidth : 1.0f;
    float sy = (sym.nominalHeight > 0.001f) ? h / sym.nominalHeight : 1.0f;

    out << "\\begin{scope}[shift={(" << fmt(tx, 3) << "," << fmt(ty, 3) << ")}";
    if (std::abs(rotDeg) > 0.1f) {
        out << ", rotate=" << fmt(-rotDeg, 1);
    }
    out << "]\n";

    for (const auto& path : sym.paths) {
        // Determine draw style
        if (path.filled && path.stroked) {
            out << "  \\filldraw[draw=" << colorName
                << ", fill=" << colorName << "!" << static_cast<int>(path.fillOpacity * 100);
            if (path.isDashed) out << ", dashed";
            out << ", line width=" << fmt(path.strokeWidth * 0.4f, 1) << "pt]";
        } else if (path.filled) {
            out << "  \\fill[" << colorName << "!" << static_cast<int>(path.fillOpacity * 100) << "]";
        } else {
            out << "  \\draw[" << colorName;
            if (path.isDashed) out << ", dashed";
            out << ", line width=" << fmt(path.strokeWidth * 0.4f, 1) << "pt]";
        }

        bool first = true;
        for (const auto& seg : path.segments) {
            float px = seg.p.x * sx;
            float py = seg.p.y * sy;
            switch (seg.cmd) {
                case PathCmd::MoveTo:
                    if (!first) out << " ";
                    out << " (" << fmt(px, 3) << "," << fmt(py, 3) << ")";
                    first = false;
                    break;
                case PathCmd::LineTo:
                    out << " -- (" << fmt(px, 3) << "," << fmt(py, 3) << ")";
                    break;
                case PathCmd::ArcTo: {
                    // TikZ arc approximation: just use line segments for arcs
                    // (TikZ arcs use center-parameterized form which doesn't map cleanly)
                    out << " -- (" << fmt(px, 3) << "," << fmt(py, 3) << ")";
                    break;
                }
                case PathCmd::Close:
                    out << " -- cycle";
                    break;
            }
        }
        out << ";\n";
    }

    out << "\\end{scope}\n";
    return out.str();
}

// ============ Optical Axis Detection ============

OpticalAxis detectOpticalAxis(const std::vector<std::unique_ptr<Element>>& elements) {
    OpticalAxis result;
    result.valid = false;

    std::vector<glm::vec2> positions;
    for (const auto& e : elements) {
        if (!e->visible) continue;
        positions.push_back({e->transform.position.x, e->transform.position.z});
    }
    if (positions.size() < 2) return result;

    // Compute mean
    float meanX = 0, meanZ = 0;
    for (auto& p : positions) { meanX += p.x; meanZ += p.y; }
    meanX /= positions.size();
    meanZ /= positions.size();

    // Compute variance along X and Z
    float varX = 0, varZ = 0;
    for (auto& p : positions) {
        varX += (p.x - meanX) * (p.x - meanX);
        varZ += (p.y - meanZ) * (p.y - meanZ);
    }

    float totalVar = varX + varZ;
    if (totalVar < 100.0f) return result; // less than 10mm spread

    bool axisIsX = (varX >= varZ);

    // Project onto dominant axis, find extent
    float minProj = FLT_MAX, maxProj = -FLT_MAX;
    float perpMean = 0;
    for (auto& p : positions) {
        float proj = axisIsX ? p.x : p.y;
        float perp = axisIsX ? p.y : p.x;
        if (proj < minProj) minProj = proj;
        if (proj > maxProj) maxProj = proj;
        perpMean += perp;
    }
    perpMean /= positions.size();

    // Extend 15% beyond outermost elements
    float extent = maxProj - minProj;
    float pad = std::max(extent * 0.15f, 25.0f);
    minProj -= pad;
    maxProj += pad;

    if (axisIsX) {
        result.start = {minProj, perpMean};
        result.end = {maxProj, perpMean};
    } else {
        result.start = {perpMean, minProj};
        result.end = {perpMean, maxProj};
    }
    result.valid = true;
    return result;
}

// ============ Scale Bar ============

ScaleBar chooseScaleBar(float sceneExtentMm) {
    // Target: bar is ~15% of scene extent
    float target = sceneExtentMm * 0.15f;

    // Nice round values
    const float niceValues[] = {1, 2, 5, 10, 20, 25, 50, 100, 200, 500, 1000};
    float best = 10.0f;
    float bestDiff = FLT_MAX;
    for (float v : niceValues) {
        float diff = std::abs(v - target);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = v;
        }
    }

    ScaleBar bar;
    bar.lengthMm = best;
    if (best >= 1000.0f) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << (best / 1000.0f) << " m";
        bar.labelText = oss.str();
    } else {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << best << " mm";
        bar.labelText = oss.str();
    }
    return bar;
}

std::string renderScaleBarSvg(const ScaleBar& bar, float scale,
                               float x, float y) {
    std::ostringstream out;
    float barLenPx = bar.lengthMm * scale;

    out << "<g id=\"scale-bar\" transform=\"translate(" << fmt(x) << "," << fmt(y) << ")\">\n";
    out << "  <line x1=\"0\" y1=\"0\" x2=\"" << fmt(barLenPx) << "\" y2=\"0\" "
        << "stroke=\"black\" stroke-width=\"1.5\" />\n";
    // End caps
    out << "  <line x1=\"0\" y1=\"-4\" x2=\"0\" y2=\"4\" stroke=\"black\" stroke-width=\"1\" />\n";
    out << "  <line x1=\"" << fmt(barLenPx) << "\" y1=\"-4\" x2=\""
        << fmt(barLenPx) << "\" y2=\"4\" stroke=\"black\" stroke-width=\"1\" />\n";
    // Label
    out << "  <text x=\"" << fmt(barLenPx * 0.5f) << "\" y=\"14\" "
        << "text-anchor=\"middle\" font-size=\"10\" font-family=\"serif\" fill=\"black\">"
        << bar.labelText << "</text>\n";
    out << "</g>\n";
    return out.str();
}

std::string renderScaleBarTikz(const ScaleBar& bar, float scale,
                                float x, float y) {
    std::ostringstream out;
    float barLenCm = bar.lengthMm * scale;

    out << "% Scale bar\n";
    out << "\\draw[thick] (" << fmt(x, 3) << "," << fmt(y, 3) << ") -- ++("
        << fmt(barLenCm, 3) << ",0);\n";
    out << "\\draw (" << fmt(x, 3) << "," << fmt(y - 0.1f, 3) << ") -- ++(0,0.2);\n";
    out << "\\draw (" << fmt(x + barLenCm, 3) << "," << fmt(y - 0.1f, 3) << ") -- ++(0,0.2);\n";
    out << "\\node[below] at (" << fmt(x + barLenCm * 0.5f, 3) << "," << fmt(y, 3) << ") "
        << "{\\small " << bar.labelText << "};\n";
    return out.str();
}

} // namespace opticsketch
