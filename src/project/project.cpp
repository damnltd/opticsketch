#include "project/project.h"
#include "scene/scene.h"
#include "elements/element.h"
#include "elements/basic_elements.h"
#include "elements/annotation.h"
#include "render/beam.h"
#include "elements/measurement.h"
#include "style/scene_style.h"
#include "camera/camera.h"
#include "scene/group.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

static void skipBOM(std::istream& in) {
    char buf[3] = {0};
    in.read(buf, 3);
    if (in.gcount() == 3 &&
        (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF)
        return;
    in.clear();
    in.seekg(0);
}

static void trim(std::string& s) {
    auto it = std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    s.erase(s.begin(), it);
    it = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    s.erase(it, s.end());
}

// Encode text for single-line storage (newlines become literal \n)
static std::string encodeText(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\n') out += "\\n";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

static std::string decodeText(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            if (s[i+1] == 'n') { out += '\n'; i++; }
            else if (s[i+1] == '\\') { out += '\\'; i++; }
            else out += s[i];
        } else {
            out += s[i];
        }
    }
    return out;
}

namespace opticsketch {

static const char* typeToString(ElementType t) {
    switch (t) {
        case ElementType::Laser:         return "Laser";
        case ElementType::Mirror:       return "Mirror";
        case ElementType::Lens:         return "Lens";
        case ElementType::BeamSplitter:  return "BeamSplitter";
        case ElementType::Detector:     return "Detector";
        case ElementType::Filter:       return "Filter";
        case ElementType::Aperture:     return "Aperture";
        case ElementType::Prism:        return "Prism";
        case ElementType::PrismRA:      return "PrismRA";
        case ElementType::Grating:      return "Grating";
        case ElementType::FiberCoupler: return "FiberCoupler";
        case ElementType::Screen:       return "Screen";
        case ElementType::Mount:        return "Mount";
        case ElementType::ImportedMesh: return "ImportedMesh";
        default:                         return "Laser";
    }
}

static ElementType stringToType(const std::string& s) {
    if (s == "Mirror") return ElementType::Mirror;
    if (s == "Lens") return ElementType::Lens;
    if (s == "BeamSplitter") return ElementType::BeamSplitter;
    if (s == "Detector") return ElementType::Detector;
    if (s == "Filter") return ElementType::Filter;
    if (s == "Aperture") return ElementType::Aperture;
    if (s == "Prism") return ElementType::Prism;
    if (s == "PrismRA") return ElementType::PrismRA;
    if (s == "Grating") return ElementType::Grating;
    if (s == "FiberCoupler") return ElementType::FiberCoupler;
    if (s == "Screen") return ElementType::Screen;
    if (s == "Mount") return ElementType::Mount;
    if (s == "ImportedMesh") return ElementType::ImportedMesh;
    return ElementType::Laser;
}

bool saveProject(const std::string& path, Scene* scene, SceneStyle* style) {
    if (!scene) return false;
    std::ofstream f(path);
    if (!f) return false;

    f << "optsk 1\n";
    for (const auto& elem : scene->getElements()) {
        if (!elem) continue;
        const Element& e = *elem;
        f << "element\n";
        f << "type " << typeToString(e.type) << "\n";
        f << "id " << e.id << "\n";
        f << "label " << e.label << "\n";
        f << "position " << e.transform.position.x << " " << e.transform.position.y << " " << e.transform.position.z << "\n";
        f << "rotation " << e.transform.rotation.x << " " << e.transform.rotation.y << " " << e.transform.rotation.z << " " << e.transform.rotation.w << "\n";
        f << "scale " << e.transform.scale.x << " " << e.transform.scale.y << " " << e.transform.scale.z << "\n";
        f << "visible " << (e.visible ? 1 : 0) << "\n";
        f << "locked " << (e.locked ? 1 : 0) << "\n";
        f << "showlabel " << (e.showLabel ? 1 : 0) << "\n";
        f << "layer " << e.layer << "\n";
        // Optical properties
        f << "opticaltype " << static_cast<int>(e.optics.opticalType) << "\n";
        f << "ior " << e.optics.ior << "\n";
        f << "reflectivity " << e.optics.reflectivity << "\n";
        f << "transmissivity " << e.optics.transmissivity << "\n";
        f << "focallength " << e.optics.focalLength << "\n";
        f << "curvature " << e.optics.curvatureR1 << " " << e.optics.curvatureR2 << "\n";
        f << "aperturedia " << e.optics.apertureDiameter << "\n";
        f << "gratingdensity " << e.optics.gratingLineDensity << "\n";
        f << "filtercolor " << e.optics.filterColor.x << " " << e.optics.filterColor.y << " " << e.optics.filterColor.z << "\n";
        f << "cauchyb " << e.optics.cauchyB << "\n";
        f << "sourceraycount " << e.optics.sourceRayCount << "\n";
        f << "sourcebeamwidth " << e.optics.sourceBeamWidth << "\n";
        f << "sourcewhitelight " << (e.optics.sourceIsWhiteLight ? 1 : 0) << "\n";
        // Material properties
        f << "metallic " << e.material.metallic << "\n";
        f << "roughness " << e.material.roughness << "\n";
        f << "transparency " << e.material.transparency << "\n";
        f << "fresnelior " << e.material.fresnelIOR << "\n";
        if (e.type == ElementType::ImportedMesh && !e.meshSourcePath.empty()) {
            f << "meshpath " << e.meshSourcePath << "\n";
        }
        f << "end\n";
    }
    // Save beams
    for (const auto& beam : scene->getBeams()) {
        if (!beam) continue;
        const Beam& b = *beam;
        f << "beam\n";
        f << "id " << b.id << "\n";
        f << "label " << b.label << "\n";
        f << "start " << b.start.x << " " << b.start.y << " " << b.start.z << "\n";
        f << "end " << b.end.x << " " << b.end.y << " " << b.end.z << "\n";
        f << "color " << b.color.x << " " << b.color.y << " " << b.color.z << "\n";
        f << "width " << b.width << "\n";
        f << "visible " << (b.visible ? 1 : 0) << "\n";
        f << "layer " << b.layer << "\n";
        if (b.isTraced) {
            f << "traced 1\n";
            f << "sourceid " << b.sourceElementId << "\n";
        }
        if (b.isGaussian) {
            f << "gaussian 1\n";
            f << "waist " << b.waistW0 << "\n";
            f << "wavelength " << b.wavelength << "\n";
            f << "waistpos " << b.waistPosition << "\n";
        }
        f << "end\n";
    }
    // Save annotations
    for (const auto& ann : scene->getAnnotations()) {
        if (!ann) continue;
        const Annotation& a = *ann;
        f << "annotation\n";
        f << "id " << a.id << "\n";
        f << "label " << a.label << "\n";
        f << "text " << encodeText(a.text) << "\n";
        f << "position " << a.position.x << " " << a.position.y << " " << a.position.z << "\n";
        f << "color " << a.color.x << " " << a.color.y << " " << a.color.z << "\n";
        f << "fontsize " << a.fontSize << "\n";
        f << "visible " << (a.visible ? 1 : 0) << "\n";
        f << "layer " << a.layer << "\n";
        f << "end\n";
    }
    // Save measurements
    for (const auto& meas : scene->getMeasurements()) {
        if (!meas) continue;
        const Measurement& m = *meas;
        f << "measurement\n";
        f << "id " << m.id << "\n";
        f << "label " << m.label << "\n";
        f << "start " << m.startPoint.x << " " << m.startPoint.y << " " << m.startPoint.z << "\n";
        f << "end " << m.endPoint.x << " " << m.endPoint.y << " " << m.endPoint.z << "\n";
        f << "color " << m.color.x << " " << m.color.y << " " << m.color.z << "\n";
        f << "fontsize " << m.fontSize << "\n";
        f << "visible " << (m.visible ? 1 : 0) << "\n";
        f << "layer " << m.layer << "\n";
        f << "end\n";
    }
    // Save groups
    for (const auto& g : scene->getGroups()) {
        f << "group\n";
        f << "id " << g.id << "\n";
        f << "name " << g.name << "\n";
        f << "members";
        for (const auto& mid : g.memberIds)
            f << " " << mid;
        f << "\n";
        f << "end\n";
    }
    // Save style
    if (style) {
        f << "style\n";
        f << "rendermode " << static_cast<int>(style->renderMode) << "\n";
        f << "bgcolor " << style->bgColor.x << " " << style->bgColor.y << " " << style->bgColor.z << "\n";
        f << "gridcolor " << style->gridColor.x << " " << style->gridColor.y << " " << style->gridColor.z << "\n";
        f << "gridalpha " << style->gridAlpha << "\n";
        f << "wireframecolor " << style->wireframeColor.x << " " << style->wireframeColor.y << " " << style->wireframeColor.z << "\n";
        f << "selbrightness " << style->selectionBrightness << "\n";
        f << "ambient " << style->ambientStrength << "\n";
        f << "specular " << style->specularStrength << "\n";
        f << "shininess " << style->specularShininess << "\n";
        for (int i = 0; i < kElementTypeCount; i++) {
            f << "elemcolor " << i << " " << style->elementColors[i].x << " " << style->elementColors[i].y << " " << style->elementColors[i].z << "\n";
        }
        f << "snaptogrid " << (style->snapToGrid ? 1 : 0) << "\n";
        f << "snapgridspacing " << style->gridSpacing << "\n";
        f << "snaptoelem " << (style->snapToElement ? 1 : 0) << "\n";
        f << "snapelemradius " << style->elementSnapRadius << "\n";
        f << "snaptobeam " << (style->snapToBeam ? 1 : 0) << "\n";
        f << "snapbeamradius " << style->beamSnapRadius << "\n";
        f << "autoorientbeam " << (style->autoOrientToBeam ? 1 : 0) << "\n";
        f << "showfocalpoints " << (style->showFocalPoints ? 1 : 0) << "\n";
        f << "bloomthreshold " << style->bloomThreshold << "\n";
        f << "bloomintensity " << style->bloomIntensity << "\n";
        f << "bloomblurpasses " << style->bloomBlurPasses << "\n";
        f << "bgmode " << static_cast<int>(style->bgMode) << "\n";
        f << "bggradtop " << style->bgGradientTop.x << " " << style->bgGradientTop.y << " " << style->bgGradientTop.z << "\n";
        f << "bggradbot " << style->bgGradientBottom.x << " " << style->bgGradientBottom.y << " " << style->bgGradientBottom.z << "\n";
        if (!style->hdriPath.empty()) {
            f << "hdripath " << style->hdriPath << "\n";
        }
        f << "hdriintensity " << style->hdriIntensity << "\n";
        f << "hdrirotation " << style->hdriRotation << "\n";
        f << "end\n";
    }
    // Save view presets
    for (const auto& vp : scene->getViewPresets()) {
        f << "viewpreset\n";
        f << "name " << vp.name << "\n";
        f << "mode " << static_cast<int>(vp.mode) << "\n";
        f << "position " << vp.position.x << " " << vp.position.y << " " << vp.position.z << "\n";
        f << "target " << vp.target.x << " " << vp.target.y << " " << vp.target.z << "\n";
        f << "up " << vp.up.x << " " << vp.up.y << " " << vp.up.z << "\n";
        f << "fov " << vp.fov << "\n";
        f << "orthosize " << vp.orthoSize << "\n";
        f << "distance " << vp.distance << "\n";
        f << "azimuth " << vp.azimuth << "\n";
        f << "elevation " << vp.elevation << "\n";
        f << "end\n";
    }
    f.flush();
    return f.good();
}

static bool parseElementBlock(std::istream& in, Scene* scene) {
    std::string typeStr = "Laser", id, label, meshpath;
    float px = 0, py = 0, pz = 0;
    float qx = 0, qy = 0, qz = 0, qw = 1;
    float sx = 1, sy = 1, sz = 1;
    int visible = 1, locked = 0, showlabel = 1, layer = 0;
    // Optical properties
    int opticaltype = -1;  // -1 means not specified (use factory defaults)
    float ior = -1, reflectivity = -1, transmissivity = -1;
    float focallength = -1, curvR1 = 0, curvR2 = 0;
    bool hasCurvature = false;
    float aperturedia = -1;
    float gratingdensity = -1;
    float filterColorR = -1, filterColorG = -1, filterColorB = -1;
    bool hasFilterColor = false;
    float cauchyb = -1;
    int sourceraycount = -1;
    float sourcebeamwidth = -1;
    int sourcewhitelight = -1;
    // Material properties
    float metallic = -1, roughness = -1, transparency = -1, fresnelior = -1;

    std::string line;
    while (std::getline(in, line)) {
        trim(line);
        if (line.empty()) continue;
        if (line == "end") break;

        if (line.compare(0, 5, "type ") == 0) {
            typeStr = line.substr(5);
            trim(typeStr);
        } else if (line.compare(0, 3, "id ") == 0) {
            id = line.substr(3);
            trim(id);
        } else if (line.compare(0, 6, "label ") == 0) {
            label = line.substr(6);
            trim(label);
        } else if (line.compare(0, 9, "position ") == 0) {
            std::istringstream ls(line.substr(9));
            ls >> px >> py >> pz;
        } else if (line.compare(0, 9, "rotation ") == 0) {
            std::istringstream ls(line.substr(9));
            ls >> qx >> qy >> qz >> qw;
        } else if (line.compare(0, 6, "scale ") == 0) {
            std::istringstream ls(line.substr(6));
            ls >> sx >> sy >> sz;
        } else if (line.compare(0, 8, "visible ") == 0) {
            visible = std::stoi(line.substr(8));
        } else if (line.compare(0, 7, "locked ") == 0) {
            locked = std::stoi(line.substr(7));
        } else if (line.compare(0, 10, "showlabel ") == 0) {
            showlabel = std::stoi(line.substr(10));
        } else if (line.compare(0, 6, "layer ") == 0) {
            layer = std::stoi(line.substr(6));
        } else if (line.compare(0, 9, "meshpath ") == 0) {
            meshpath = line.substr(9);
            trim(meshpath);
        } else if (line.compare(0, 12, "opticaltype ") == 0) {
            opticaltype = std::stoi(line.substr(12));
        } else if (line.compare(0, 4, "ior ") == 0) {
            ior = std::stof(line.substr(4));
        } else if (line.compare(0, 14, "reflectivity ") == 0) {
            reflectivity = std::stof(line.substr(14));
        } else if (line.compare(0, 15, "transmissivity ") == 0) {
            transmissivity = std::stof(line.substr(15));
        } else if (line.compare(0, 12, "focallength ") == 0) {
            focallength = std::stof(line.substr(12));
        } else if (line.compare(0, 10, "curvature ") == 0) {
            std::istringstream ls(line.substr(10));
            ls >> curvR1 >> curvR2;
            hasCurvature = true;
        } else if (line.compare(0, 12, "aperturedia ") == 0) {
            aperturedia = std::stof(line.substr(12));
        } else if (line.compare(0, 15, "gratingdensity ") == 0) {
            gratingdensity = std::stof(line.substr(15));
        } else if (line.compare(0, 12, "filtercolor ") == 0) {
            std::istringstream ls(line.substr(12));
            ls >> filterColorR >> filterColorG >> filterColorB;
            hasFilterColor = true;
        } else if (line.compare(0, 8, "cauchyb ") == 0) {
            cauchyb = std::stof(line.substr(8));
        } else if (line.compare(0, 15, "sourceraycount ") == 0) {
            sourceraycount = std::stoi(line.substr(15));
        } else if (line.compare(0, 16, "sourcebeamwidth ") == 0) {
            sourcebeamwidth = std::stof(line.substr(16));
        } else if (line.compare(0, 17, "sourcewhitelight ") == 0) {
            sourcewhitelight = std::stoi(line.substr(17));
        } else if (line.compare(0, 9, "metallic ") == 0) {
            metallic = std::stof(line.substr(9));
        } else if (line.compare(0, 10, "roughness ") == 0) {
            roughness = std::stof(line.substr(10));
        } else if (line.compare(0, 13, "transparency ") == 0) {
            transparency = std::stof(line.substr(13));
        } else if (line.compare(0, 10, "fresnelior ") == 0) {
            fresnelior = std::stof(line.substr(10));
        }
    }

    ElementType type = stringToType(typeStr);
    std::unique_ptr<Element> elem;
    if (type == ElementType::ImportedMesh && !meshpath.empty()) {
        elem = createMeshElement(meshpath, id);
    } else {
        elem = createElement(type, id);
    }
    if (!elem) return false;

    elem->label = label;
    elem->transform.position = glm::vec3(px, py, pz);
    elem->transform.rotation = glm::quat(qw, qx, qy, qz);
    elem->transform.scale = glm::vec3(sx, sy, sz);
    elem->visible = (visible != 0);
    elem->locked = (locked != 0);
    elem->showLabel = (showlabel != 0);
    elem->layer = layer;

    // Override optical properties if specified in file
    if (opticaltype >= 0 && opticaltype <= 10)
        elem->optics.opticalType = static_cast<OpticalType>(opticaltype);
    if (ior >= 0) elem->optics.ior = ior;
    if (reflectivity >= 0) elem->optics.reflectivity = reflectivity;
    if (transmissivity >= 0) elem->optics.transmissivity = transmissivity;
    if (focallength >= 0) elem->optics.focalLength = focallength;
    if (hasCurvature) { elem->optics.curvatureR1 = curvR1; elem->optics.curvatureR2 = curvR2; }
    if (aperturedia >= 0) elem->optics.apertureDiameter = aperturedia;
    if (gratingdensity >= 0) elem->optics.gratingLineDensity = gratingdensity;
    if (hasFilterColor) elem->optics.filterColor = glm::vec3(filterColorR, filterColorG, filterColorB);
    if (cauchyb >= 0) elem->optics.cauchyB = cauchyb;
    if (sourceraycount >= 0) elem->optics.sourceRayCount = sourceraycount;
    if (sourcebeamwidth >= 0) elem->optics.sourceBeamWidth = sourcebeamwidth;
    if (sourcewhitelight >= 0) elem->optics.sourceIsWhiteLight = (sourcewhitelight != 0);

    // Override material properties if specified
    if (metallic >= 0) elem->material.metallic = metallic;
    if (roughness >= 0) elem->material.roughness = roughness;
    if (transparency >= 0) elem->material.transparency = transparency;
    if (fresnelior >= 0) elem->material.fresnelIOR = fresnelior;

    scene->addElement(std::move(elem));
    return true;
}

static bool parseBeamBlock(std::istream& in, Scene* scene) {
    std::string id, label;
    float sx = 0, sy = 0, sz = 0;
    float ex = 0, ey = 0, ez = 0;
    float cr = 1.0f, cg = 0.2f, cb = 0.2f;
    float width = 2.0f;
    int visible = 1, layer = 0;
    int traced = 0;
    std::string sourceid;
    int gaussian = 0;
    float waist = 0.001f, wl = 633e-9f, waistpos = 0.0f;

    std::string line;
    while (std::getline(in, line)) {
        trim(line);
        if (line.empty()) continue;
        if (line == "end") break;

        if (line.compare(0, 3, "id ") == 0) {
            id = line.substr(3);
            trim(id);
        } else if (line.compare(0, 6, "label ") == 0) {
            label = line.substr(6);
            trim(label);
        } else if (line.compare(0, 6, "start ") == 0) {
            std::istringstream ls(line.substr(6));
            ls >> sx >> sy >> sz;
        } else if (line.compare(0, 4, "end ") == 0 && line.length() > 4) {
            std::istringstream ls(line.substr(4));
            ls >> ex >> ey >> ez;
        } else if (line.compare(0, 6, "color ") == 0) {
            std::istringstream ls(line.substr(6));
            ls >> cr >> cg >> cb;
        } else if (line.compare(0, 6, "width ") == 0) {
            width = std::stof(line.substr(6));
        } else if (line.compare(0, 8, "visible ") == 0) {
            visible = std::stoi(line.substr(8));
        } else if (line.compare(0, 6, "layer ") == 0) {
            layer = std::stoi(line.substr(6));
        } else if (line.compare(0, 7, "traced ") == 0) {
            traced = std::stoi(line.substr(7));
        } else if (line.compare(0, 9, "sourceid ") == 0) {
            sourceid = line.substr(9);
            trim(sourceid);
        } else if (line.compare(0, 9, "gaussian ") == 0) {
            gaussian = std::stoi(line.substr(9));
        } else if (line.compare(0, 6, "waist ") == 0) {
            waist = std::stof(line.substr(6));
        } else if (line.compare(0, 11, "wavelength ") == 0) {
            wl = std::stof(line.substr(11));
        } else if (line.compare(0, 9, "waistpos ") == 0) {
            waistpos = std::stof(line.substr(9));
        }
    }

    auto beam = std::make_unique<Beam>(id);
    beam->label = label;
    beam->start = glm::vec3(sx, sy, sz);
    beam->end = glm::vec3(ex, ey, ez);
    beam->color = glm::vec3(cr, cg, cb);
    beam->width = width;
    beam->visible = (visible != 0);
    beam->layer = layer;
    beam->isTraced = (traced != 0);
    beam->sourceElementId = sourceid;
    beam->isGaussian = (gaussian != 0);
    beam->waistW0 = waist;
    beam->wavelength = wl;
    beam->waistPosition = waistpos;

    scene->addBeam(std::move(beam));
    return true;
}

static bool parseAnnotationBlock(std::istream& in, Scene* scene) {
    std::string id, label, text;
    float px = 0, py = 0, pz = 0;
    float cr = 0.95f, cg = 0.95f, cb = 0.85f;
    float fontSize = 14.0f;
    int visible = 1, layer = 0;

    std::string line;
    while (std::getline(in, line)) {
        trim(line);
        if (line.empty()) continue;
        if (line == "end") break;

        if (line.compare(0, 3, "id ") == 0) {
            id = line.substr(3);
            trim(id);
        } else if (line.compare(0, 6, "label ") == 0) {
            label = line.substr(6);
            trim(label);
        } else if (line.compare(0, 5, "text ") == 0) {
            text = decodeText(line.substr(5));
        } else if (line.compare(0, 9, "position ") == 0) {
            std::istringstream ls(line.substr(9));
            ls >> px >> py >> pz;
        } else if (line.compare(0, 6, "color ") == 0) {
            std::istringstream ls(line.substr(6));
            ls >> cr >> cg >> cb;
        } else if (line.compare(0, 9, "fontsize ") == 0) {
            fontSize = std::stof(line.substr(9));
        } else if (line.compare(0, 8, "visible ") == 0) {
            visible = std::stoi(line.substr(8));
        } else if (line.compare(0, 6, "layer ") == 0) {
            layer = std::stoi(line.substr(6));
        }
    }

    auto ann = std::make_unique<Annotation>(id.empty() ? Annotation::generateId() : id);
    ann->label = label;
    ann->text = text;
    ann->position = glm::vec3(px, py, pz);
    ann->color = glm::vec3(cr, cg, cb);
    ann->fontSize = fontSize;
    ann->visible = (visible != 0);
    ann->layer = layer;

    scene->addAnnotation(std::move(ann));
    return true;
}

static bool parseStyleBlock(std::istream& in, SceneStyle* style) {
    if (!style) {
        // Skip the block if no style pointer
        std::string line;
        while (std::getline(in, line)) {
            trim(line);
            if (line == "end") break;
        }
        return true;
    }

    std::string line;
    while (std::getline(in, line)) {
        trim(line);
        if (line.empty()) continue;
        if (line == "end") break;

        if (line.compare(0, 8, "bgcolor ") == 0) {
            std::istringstream ls(line.substr(8));
            ls >> style->bgColor.x >> style->bgColor.y >> style->bgColor.z;
        } else if (line.compare(0, 10, "gridcolor ") == 0) {
            std::istringstream ls(line.substr(10));
            ls >> style->gridColor.x >> style->gridColor.y >> style->gridColor.z;
        } else if (line.compare(0, 10, "gridalpha ") == 0) {
            style->gridAlpha = std::stof(line.substr(10));
        } else if (line.compare(0, 15, "wireframecolor ") == 0) {
            std::istringstream ls(line.substr(15));
            ls >> style->wireframeColor.x >> style->wireframeColor.y >> style->wireframeColor.z;
        } else if (line.compare(0, 14, "selbrightness ") == 0) {
            style->selectionBrightness = std::stof(line.substr(14));
        } else if (line.compare(0, 8, "ambient ") == 0) {
            style->ambientStrength = std::stof(line.substr(8));
        } else if (line.compare(0, 9, "specular ") == 0) {
            style->specularStrength = std::stof(line.substr(9));
        } else if (line.compare(0, 10, "shininess ") == 0) {
            style->specularShininess = std::stof(line.substr(10));
        } else if (line.compare(0, 10, "elemcolor ") == 0) {
            std::istringstream ls(line.substr(10));
            int idx;
            float r, g, b;
            ls >> idx >> r >> g >> b;
            if (idx >= 0 && idx < kElementTypeCount) {
                style->elementColors[idx] = glm::vec3(r, g, b);
            }
        } else if (line.compare(0, 11, "snaptogrid ") == 0) {
            style->snapToGrid = (std::stoi(line.substr(11)) != 0);
        } else if (line.compare(0, 16, "snapgridspacing ") == 0) {
            style->gridSpacing = std::stof(line.substr(16));
        } else if (line.compare(0, 11, "snaptoelem ") == 0) {
            style->snapToElement = (std::stoi(line.substr(11)) != 0);
        } else if (line.compare(0, 15, "snapelemradius ") == 0) {
            style->elementSnapRadius = std::stof(line.substr(15));
        } else if (line.compare(0, 11, "snaptobeam ") == 0) {
            style->snapToBeam = (std::stoi(line.substr(11)) != 0);
        } else if (line.compare(0, 15, "snapbeamradius ") == 0) {
            style->beamSnapRadius = std::stof(line.substr(15));
        } else if (line.compare(0, 15, "autoorientbeam ") == 0) {
            style->autoOrientToBeam = (std::stoi(line.substr(15)) != 0);
        } else if (line.compare(0, 11, "rendermode ") == 0) {
            int rm = std::stoi(line.substr(11));
            if (rm >= 0 && rm <= 2) style->renderMode = static_cast<RenderMode>(rm);
        } else if (line.compare(0, 16, "showfocalpoints ") == 0) {
            style->showFocalPoints = (std::stoi(line.substr(16)) != 0);
        } else if (line.compare(0, 15, "bloomthreshold ") == 0) {
            style->bloomThreshold = std::stof(line.substr(15));
        } else if (line.compare(0, 15, "bloomintensity ") == 0) {
            style->bloomIntensity = std::stof(line.substr(15));
        } else if (line.compare(0, 16, "bloomblurpasses ") == 0) {
            style->bloomBlurPasses = std::stoi(line.substr(16));
        } else if (line.compare(0, 7, "bgmode ") == 0) {
            int m = std::stoi(line.substr(7));
            if (m >= 0 && m <= 1) style->bgMode = static_cast<BackgroundMode>(m);
        } else if (line.compare(0, 10, "bggradtop ") == 0) {
            std::istringstream ls(line.substr(10));
            ls >> style->bgGradientTop.x >> style->bgGradientTop.y >> style->bgGradientTop.z;
        } else if (line.compare(0, 10, "bggradbot ") == 0) {
            std::istringstream ls(line.substr(10));
            ls >> style->bgGradientBottom.x >> style->bgGradientBottom.y >> style->bgGradientBottom.z;
        } else if (line.compare(0, 9, "hdripath ") == 0) {
            style->hdriPath = line.substr(9);
            // Trim whitespace
            while (!style->hdriPath.empty() && (style->hdriPath.back() == ' ' || style->hdriPath.back() == '\r'))
                style->hdriPath.pop_back();
        } else if (line.compare(0, 14, "hdriintensity ") == 0) {
            style->hdriIntensity = std::stof(line.substr(14));
        } else if (line.compare(0, 13, "hdrirotation ") == 0) {
            style->hdriRotation = std::stof(line.substr(13));
        }
    }
    return true;
}

static bool parseGroupBlock(std::istream& in, Scene* scene) {
    Group g;
    std::string line;
    while (std::getline(in, line)) {
        trim(line);
        if (line.empty()) continue;
        if (line == "end") break;

        if (line.compare(0, 3, "id ") == 0) {
            g.id = line.substr(3); trim(g.id);
        } else if (line.compare(0, 5, "name ") == 0) {
            g.name = line.substr(5); trim(g.name);
        } else if (line.compare(0, 7, "members") == 0) {
            std::istringstream ls(line.substr(7));
            std::string mid;
            while (ls >> mid)
                g.memberIds.push_back(mid);
        }
    }
    if (g.id.empty()) g.id = Group::generateId();
    scene->addGroup(g);
    return true;
}

static bool parseMeasurementBlock(std::istream& in, Scene* scene) {
    std::string id, label;
    float sx = 0, sy = 0, sz = 0;
    float ex = 0, ey = 0, ez = 0;
    float cr = 0.9f, cg = 0.9f, cb = 0.3f;
    float fontSize = 12.0f;
    int visible = 1, layer = 0;

    std::string line;
    while (std::getline(in, line)) {
        trim(line);
        if (line.empty()) continue;
        if (line == "end") break;

        if (line.compare(0, 3, "id ") == 0) {
            id = line.substr(3); trim(id);
        } else if (line.compare(0, 6, "label ") == 0) {
            label = line.substr(6); trim(label);
        } else if (line.compare(0, 6, "start ") == 0) {
            std::istringstream ls(line.substr(6));
            ls >> sx >> sy >> sz;
        } else if (line.compare(0, 4, "end ") == 0 && line.length() > 4) {
            std::istringstream ls(line.substr(4));
            ls >> ex >> ey >> ez;
        } else if (line.compare(0, 6, "color ") == 0) {
            std::istringstream ls(line.substr(6));
            ls >> cr >> cg >> cb;
        } else if (line.compare(0, 9, "fontsize ") == 0) {
            fontSize = std::stof(line.substr(9));
        } else if (line.compare(0, 8, "visible ") == 0) {
            visible = std::stoi(line.substr(8));
        } else if (line.compare(0, 6, "layer ") == 0) {
            layer = std::stoi(line.substr(6));
        }
    }

    auto meas = std::make_unique<Measurement>(id.empty() ? Measurement::generateId() : id);
    meas->label = label;
    meas->startPoint = glm::vec3(sx, sy, sz);
    meas->endPoint = glm::vec3(ex, ey, ez);
    meas->color = glm::vec3(cr, cg, cb);
    meas->fontSize = fontSize;
    meas->visible = (visible != 0);
    meas->layer = layer;

    scene->addMeasurement(std::move(meas));
    return true;
}

static bool parseViewPresetBlock(std::istream& in, Scene* scene) {
    ViewPreset vp;
    std::string line;
    while (std::getline(in, line)) {
        trim(line);
        if (line.empty()) continue;
        if (line == "end") break;

        if (line.compare(0, 5, "name ") == 0) {
            vp.name = line.substr(5);
            trim(vp.name);
        } else if (line.compare(0, 5, "mode ") == 0) {
            vp.mode = static_cast<CameraMode>(std::stoi(line.substr(5)));
        } else if (line.compare(0, 9, "position ") == 0) {
            std::istringstream ls(line.substr(9));
            ls >> vp.position.x >> vp.position.y >> vp.position.z;
        } else if (line.compare(0, 7, "target ") == 0) {
            std::istringstream ls(line.substr(7));
            ls >> vp.target.x >> vp.target.y >> vp.target.z;
        } else if (line.compare(0, 3, "up ") == 0) {
            std::istringstream ls(line.substr(3));
            ls >> vp.up.x >> vp.up.y >> vp.up.z;
        } else if (line.compare(0, 4, "fov ") == 0) {
            vp.fov = std::stof(line.substr(4));
        } else if (line.compare(0, 10, "orthosize ") == 0) {
            vp.orthoSize = std::stof(line.substr(10));
        } else if (line.compare(0, 9, "distance ") == 0) {
            vp.distance = std::stof(line.substr(9));
        } else if (line.compare(0, 8, "azimuth ") == 0) {
            vp.azimuth = std::stof(line.substr(8));
        } else if (line.compare(0, 10, "elevation ") == 0) {
            vp.elevation = std::stof(line.substr(10));
        }
    }
    scene->addViewPreset(vp);
    return true;
}

bool loadProject(const std::string& path, Scene* scene, SceneStyle* style) {
    if (!scene) return false;
    std::ifstream f(path);
    if (!f) return false;

    skipBOM(f);
    std::string line;
    if (!std::getline(f, line)) return false;
    trim(line);
    // Require first line to be "optsk " (version); only clear scene after we know the file is valid
    if (line.size() < 5 || line.compare(0, 5, "optsk") != 0) return false;

    scene->clear();

    while (std::getline(f, line)) {
        trim(line);
        if (line == "element") {
            if (!parseElementBlock(f, scene)) return false;
        } else if (line == "beam") {
            if (!parseBeamBlock(f, scene)) return false;
        } else if (line == "annotation") {
            if (!parseAnnotationBlock(f, scene)) return false;
        } else if (line == "measurement") {
            if (!parseMeasurementBlock(f, scene)) return false;
        } else if (line == "group") {
            if (!parseGroupBlock(f, scene)) return false;
        } else if (line == "style") {
            if (!parseStyleBlock(f, style)) return false;
        } else if (line == "viewpreset") {
            if (!parseViewPresetBlock(f, scene)) return false;
        }
    }
    return true;
}

bool saveStylePreset(const std::string& path, const SceneStyle* style) {
    if (!style) return false;
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "optstyle 1\n";
    f << "style\n";
    f << "rendermode " << static_cast<int>(style->renderMode) << "\n";
    f << "bgcolor " << style->bgColor.x << " " << style->bgColor.y << " " << style->bgColor.z << "\n";
    f << "gridcolor " << style->gridColor.x << " " << style->gridColor.y << " " << style->gridColor.z << "\n";
    f << "gridalpha " << style->gridAlpha << "\n";
    f << "wireframecolor " << style->wireframeColor.x << " " << style->wireframeColor.y << " " << style->wireframeColor.z << "\n";
    f << "selbrightness " << style->selectionBrightness << "\n";
    f << "ambient " << style->ambientStrength << "\n";
    f << "specular " << style->specularStrength << "\n";
    f << "shininess " << style->specularShininess << "\n";
    for (int i = 0; i < kElementTypeCount; i++) {
        f << "elemcolor " << i << " " << style->elementColors[i].x << " " << style->elementColors[i].y << " " << style->elementColors[i].z << "\n";
    }
    f << "snaptogrid " << (style->snapToGrid ? 1 : 0) << "\n";
    f << "snapgridspacing " << style->gridSpacing << "\n";
    f << "snaptoelem " << (style->snapToElement ? 1 : 0) << "\n";
    f << "snapelemradius " << style->elementSnapRadius << "\n";
    f << "snaptobeam " << (style->snapToBeam ? 1 : 0) << "\n";
    f << "snapbeamradius " << style->beamSnapRadius << "\n";
    f << "autoorientbeam " << (style->autoOrientToBeam ? 1 : 0) << "\n";
    f << "showfocalpoints " << (style->showFocalPoints ? 1 : 0) << "\n";
    f << "bloomthreshold " << style->bloomThreshold << "\n";
    f << "bloomintensity " << style->bloomIntensity << "\n";
    f << "bloomblurpasses " << style->bloomBlurPasses << "\n";
    f << "bgmode " << static_cast<int>(style->bgMode) << "\n";
    f << "bggradtop " << style->bgGradientTop.x << " " << style->bgGradientTop.y << " " << style->bgGradientTop.z << "\n";
    f << "bggradbot " << style->bgGradientBottom.x << " " << style->bgGradientBottom.y << " " << style->bgGradientBottom.z << "\n";
    if (!style->hdriPath.empty()) {
        f << "hdripath " << style->hdriPath << "\n";
    }
    f << "hdriintensity " << style->hdriIntensity << "\n";
    f << "hdrirotation " << style->hdriRotation << "\n";
    f << "end\n";

    return f.good();
}

bool loadStylePreset(const std::string& path, SceneStyle* style) {
    if (!style) return false;
    std::ifstream f(path);
    if (!f.is_open()) return false;

    skipBOM(f);

    std::string line;
    // Read header
    if (!std::getline(f, line)) return false;
    trim(line);
    if (line != "optstyle 1") return false;

    // Read style block
    if (!std::getline(f, line)) return false;
    trim(line);
    if (line != "style") return false;

    return parseStyleBlock(f, style);
}

} // namespace opticsketch
