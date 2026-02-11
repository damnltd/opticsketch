#include "project/project.h"
#include "scene/scene.h"
#include "elements/element.h"
#include "elements/basic_elements.h"
#include "render/beam.h"
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

namespace opticsketch {

static const char* typeToString(ElementType t) {
    switch (t) {
        case ElementType::Laser:         return "Laser";
        case ElementType::Mirror:       return "Mirror";
        case ElementType::Lens:         return "Lens";
        case ElementType::BeamSplitter:  return "BeamSplitter";
        case ElementType::Detector:     return "Detector";
        default:                         return "Laser";
    }
}

static ElementType stringToType(const std::string& s) {
    if (s == "Mirror") return ElementType::Mirror;
    if (s == "Lens") return ElementType::Lens;
    if (s == "BeamSplitter") return ElementType::BeamSplitter;
    if (s == "Detector") return ElementType::Detector;
    return ElementType::Laser;
}

bool saveProject(const std::string& path, Scene* scene) {
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
        f << "layer " << e.layer << "\n";
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
        f << "end\n";
    }
    f.flush();
    return f.good();
}

static bool parseElementBlock(std::istream& in, Scene* scene) {
    std::string typeStr = "Laser", id, label;
    float px = 0, py = 0, pz = 0;
    float qx = 0, qy = 0, qz = 0, qw = 1;
    float sx = 1, sy = 1, sz = 1;
    int visible = 1, locked = 0, layer = 0;

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
        } else if (line.compare(0, 6, "layer ") == 0) {
            layer = std::stoi(line.substr(6));
        }
    }

    ElementType type = stringToType(typeStr);
    std::unique_ptr<Element> elem = createElement(type, id);
    if (!elem) return false;

    elem->label = label;
    elem->transform.position = glm::vec3(px, py, pz);
    elem->transform.rotation = glm::quat(qw, qx, qy, qz);
    elem->transform.scale = glm::vec3(sx, sy, sz);
    elem->visible = (visible != 0);
    elem->locked = (locked != 0);
    elem->layer = layer;

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

    scene->addBeam(std::move(beam));
    return true;
}

bool loadProject(const std::string& path, Scene* scene) {
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
        }
    }
    return true;
}

} // namespace opticsketch
