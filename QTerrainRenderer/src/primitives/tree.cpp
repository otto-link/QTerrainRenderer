/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <glm/gtc/constants.hpp>

#include "qtr/mesh.hpp"

namespace qtr
{

void generate_tree(Mesh &mesh,
                   float height,
                   float radius,
                   int   segments,
                   float base_y = 0.0f)
{
  std::vector<Vertex> vertices;
  std::vector<uint>   indices;

  // Apex vertex
  glm::vec3 apex(0.0f, base_y + height, 0.0f);
  glm::vec3 apex_normal(0.0f, 1.0f, 0.0f);
  glm::vec2 apex_uv(0.5f, 1.0f);
  vertices.emplace_back(apex, apex_normal, apex_uv);

  // Base circle vertices
  for (int i = 0; i < segments; ++i)
  {
    float     theta = 2.0f * glm::pi<float>() * float(i) / float(segments);
    float     x = radius * cos(theta);
    float     z = radius * sin(theta);
    glm::vec3 pos(x, base_y, z);
    glm::vec3 normal = glm::normalize(
        glm::vec3(x, radius / height, z)); // approximate normal
    glm::vec2 uv(float(i) / (segments - 1), 0.0f);
    vertices.emplace_back(pos, normal, uv);
  }

  // Create side triangles (cone)
  for (int i = 0; i < segments; ++i)
  {
    int base0 = i + 1;
    int base1 = (i + 1) % segments + 1;
    indices.push_back(0); // apex
    indices.push_back(base0);
    indices.push_back(base1);
  }

  // Optionally, add a base disk
  glm::vec3 base_normal(0.0f, -1.0f, 0.0f);
  int       base_center_idx = static_cast<int>(vertices.size());
  vertices.emplace_back(glm::vec3(0.0f, base_y, 0.0f),
                        base_normal,
                        glm::vec2(0.5f, 0.5f));

  for (int i = 0; i < segments; ++i)
  {
    int base0 = i + 1;
    int base1 = (i + 1) % segments + 1;
    indices.push_back(base_center_idx);
    indices.push_back(base1);
    indices.push_back(base0);
  }

  mesh.create(vertices, indices);
}

} // namespace qtr
