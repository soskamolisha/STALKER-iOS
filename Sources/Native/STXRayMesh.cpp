#include "STXRayMesh.h"
#include "XRayCFormGeometry.hpp"
#include <vector>

extern "C" int32_t st_xray_read_collision_mesh(const char* utf8_path,
                                                 float* xyz, uint32_t vertex_capacity,
                                                 uint32_t* indices, uint32_t triangle_capacity,
                                                 uint32_t* vertex_count, uint32_t* triangle_count) {
    if (!utf8_path || !vertex_count || !triangle_count ||
        (vertex_capacity && !xyz) || (triangle_capacity && !indices)) return -1;
    *vertex_count = 0;
    *triangle_count = 0;
    stalker::CFormHeader header;
    std::vector<stalker::CFormVertex> vertices;
    std::vector<stalker::CFormTriangle> triangles;
    // Restrict the prototype viewer to a small, bounded mesh.
    if (!stalker::readCFormGeometry(utf8_path, header, vertices, triangles, 50000, 50000))
        return -2;
    if (vertices.size() > vertex_capacity || triangles.size() > triangle_capacity) return -3;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        xyz[i * 3] = vertices[i].x;
        xyz[i * 3 + 1] = vertices[i].y;
        xyz[i * 3 + 2] = vertices[i].z;
    }
    for (std::size_t i = 0; i < triangles.size(); ++i) {
        indices[i * 3] = triangles[i].a;
        indices[i * 3 + 1] = triangles[i].b;
        indices[i * 3 + 2] = triangles[i].c;
    }
    *vertex_count = static_cast<uint32_t>(vertices.size());
    *triangle_count = static_cast<uint32_t>(triangles.size());
    return 0;
}
