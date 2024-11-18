#include "StaticMesh.h"

#include <glad/gl.h>
#include <cmath>

namespace OM3D {

extern bool audit_bindings_before_draw;

StaticMesh::StaticMesh(const MeshData& data) :
    _vertex_buffer(data.vertices),
    _index_buffer(data.indices) {
    glm::vec3 min = glm::vec3(std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
    glm::vec3 max = glm::vec3(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());;
    for (auto e : data.vertices) {
        min.x = (min.x < e.position.x) ? min.x : e.position.x;
        min.y = (min.y < e.position.y) ? min.y : e.position.y;
        min.z = (min.z < e.position.z) ? min.z : e.position.z;
        max.x = (max.x > e.position.x) ? max.x : e.position.x;
        max.y = (max.y > e.position.y) ? max.y : e.position.y;
        max.z = (max.z > e.position.z) ? max.z : e.position.z;
    }
    _center = (min + max);
    _center.x = 0.5 * _center.x;
    _center.y = 0.5 * _center.y;
    _center.z = 0.5 * _center.z;
    glm::vec3 diag = (max - min);
    _radius = std::sqrt((diag.x * diag.x + diag.y * diag.y + diag.z * diag.z)) * 0.5f;

}

void StaticMesh::draw() const {
    _vertex_buffer.bind(BufferUsage::Attribute);
    _index_buffer.bind(BufferUsage::Index);

    // Vertex position
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vertex), nullptr);
    // Vertex normal
    glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void*>(3 * sizeof(float)));
    // Vertex uv
    glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void*>(6 * sizeof(float)));
    // Tangent / bitangent sign
    glVertexAttribPointer(3, 4, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void*>(8 * sizeof(float)));
    // Vertex color
    glVertexAttribPointer(4, 3, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void*>(12 * sizeof(float)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);

    if(audit_bindings_before_draw) {
        audit_bindings();
    }

    glDrawElements(GL_TRIANGLES, int(_index_buffer.element_count()), GL_UNSIGNED_INT, nullptr);
}

}
