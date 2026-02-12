#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "render/mesh_loader.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace opticsketch {

bool loadObjFile(const std::string& path, MeshData& outData) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());
    if (!ok) {
        std::cerr << "Failed to load OBJ: " << path << "\n";
        if (!err.empty()) std::cerr << "  Error: " << err << "\n";
        return false;
    }
    if (!warn.empty()) {
        std::cerr << "OBJ warning: " << warn << "\n";
    }

    // Collect all triangulated vertices with normals
    std::vector<float> rawVertices;
    bool hasNormals = !attrib.normals.empty();

    for (const auto& shape : shapes) {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            int fv = shape.mesh.num_face_vertices[f];

            // Collect face vertices
            std::vector<glm::vec3> facePositions;
            std::vector<glm::vec3> faceNormals;
            bool faceHasNormals = true;

            for (int v = 0; v < fv; v++) {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

                glm::vec3 pos(
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]
                );
                facePositions.push_back(pos);

                if (hasNormals && idx.normal_index >= 0) {
                    glm::vec3 norm(
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2]
                    );
                    faceNormals.push_back(norm);
                } else {
                    faceHasNormals = false;
                }
            }

            // Compute face normal if normals are missing
            glm::vec3 computedNormal(0.0f, 1.0f, 0.0f);
            if (!faceHasNormals && facePositions.size() >= 3) {
                glm::vec3 e1 = facePositions[1] - facePositions[0];
                glm::vec3 e2 = facePositions[2] - facePositions[0];
                glm::vec3 n = glm::cross(e1, e2);
                float len = glm::length(n);
                if (len > 1e-8f) computedNormal = n / len;
            }

            // Triangulate (fan from first vertex)
            for (int v = 1; v + 1 < fv; v++) {
                int indices[3] = {0, v, v + 1};
                for (int i = 0; i < 3; i++) {
                    int vi = indices[i];
                    rawVertices.push_back(facePositions[vi].x);
                    rawVertices.push_back(facePositions[vi].y);
                    rawVertices.push_back(facePositions[vi].z);
                    if (faceHasNormals) {
                        rawVertices.push_back(faceNormals[vi].x);
                        rawVertices.push_back(faceNormals[vi].y);
                        rawVertices.push_back(faceNormals[vi].z);
                    } else {
                        rawVertices.push_back(computedNormal.x);
                        rawVertices.push_back(computedNormal.y);
                        rawVertices.push_back(computedNormal.z);
                    }
                }
            }

            indexOffset += fv;
        }
    }

    if (rawVertices.empty()) {
        std::cerr << "OBJ file has no geometry: " << path << "\n";
        return false;
    }

    // Compute AABB from raw positions
    glm::vec3 bmin(FLT_MAX), bmax(-FLT_MAX);
    for (size_t i = 0; i < rawVertices.size(); i += 6) {
        glm::vec3 p(rawVertices[i], rawVertices[i + 1], rawVertices[i + 2]);
        bmin = glm::min(bmin, p);
        bmax = glm::max(bmax, p);
    }

    // Center at origin and normalize to fit within a unit cube
    glm::vec3 center = (bmin + bmax) * 0.5f;
    glm::vec3 extent = bmax - bmin;
    float maxExtent = std::max({extent.x, extent.y, extent.z});
    float scale = (maxExtent > 1e-8f) ? (2.0f / maxExtent) : 1.0f;

    for (size_t i = 0; i < rawVertices.size(); i += 6) {
        rawVertices[i + 0] = (rawVertices[i + 0] - center.x) * scale;
        rawVertices[i + 1] = (rawVertices[i + 1] - center.y) * scale;
        rawVertices[i + 2] = (rawVertices[i + 2] - center.z) * scale;
        // Normals stay unchanged
    }

    // Recompute bounds after normalization
    bmin = glm::vec3(FLT_MAX);
    bmax = glm::vec3(-FLT_MAX);
    for (size_t i = 0; i < rawVertices.size(); i += 6) {
        glm::vec3 p(rawVertices[i], rawVertices[i + 1], rawVertices[i + 2]);
        bmin = glm::min(bmin, p);
        bmax = glm::max(bmax, p);
    }

    outData.vertices = std::move(rawVertices);
    outData.boundsMin = bmin;
    outData.boundsMax = bmax;
    return true;
}

} // namespace opticsketch
