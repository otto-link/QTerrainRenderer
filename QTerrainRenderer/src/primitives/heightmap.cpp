/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <glm/gtc/constants.hpp>

#include "qtr/mesh.hpp"

namespace qtr
{

void generate_heightmap(Mesh                     &mesh,
                        const std::vector<float> &data,
                        int                       width,
                        int                       height,
                        float                     x,
                        float                     y,
                        float                     z,
                        float                     lx,
                        float                     ly,
                        float                     lz,
                        bool                      add_skirt,
                        float                     add_level,
                        float                     exclude_below)
{
  std::vector<Vertex> vertices;
  std::vector<uint>   indices;
  std::vector<int>    vertex_map(width * height, -1); // maps grid idx -> vertex idx

  float hx = lx * 0.5f; // half span in X
  float hz = lz * 0.5f; // half span in Z

  float dx = lx / (width - 1);  // X step
  float dz = lz / (height - 1); // Z step

  // ---- Generate main surface vertices ----
  for (int j = 0; j < height; ++j)
    for (int i = 0; i < width; ++i)
    {
      int idx = j * width + i;

      if (data[idx] < exclude_below)
      {
        continue; // skip this vertex
      }

      glm::vec3 position(x - hx + i * dx,    // X
                         y + data[idx] * ly, // Y (heightmap modifies Y)
                         z - hz + j * dz     // Z
      );

      glm::vec3 normal(0.0f, 1.0f, 0.0f); // placeholder normal
      glm::vec2 uv(static_cast<float>(i) / (width - 1),
                   static_cast<float>(j) / (height - 1));

      int newIndex = static_cast<int>(vertices.size());
      vertex_map[idx] = newIndex;
      vertices.emplace_back(position, normal, uv);
    }

  // ---- Generate main surface indices ----
  for (int j = 0; j < height - 1; ++j)
    for (int i = 0; i < width - 1; ++i)
    {
      int idx0 = j * width + i;
      int idx1 = idx0 + 1;
      int idx2 = (j + 1) * width + i;
      int idx3 = idx2 + 1;

      // If any of these vertices were excluded, skip the quad
      if (vertex_map[idx0] < 0 || vertex_map[idx1] < 0 || vertex_map[idx2] < 0 ||
          vertex_map[idx3] < 0)
        continue;

      int topLeft = vertex_map[idx0];
      int topRight = vertex_map[idx1];
      int bottomLeft = vertex_map[idx2];
      int bottomRight = vertex_map[idx3];

      indices.push_back(topLeft);
      indices.push_back(bottomLeft);
      indices.push_back(topRight);

      indices.push_back(topRight);
      indices.push_back(bottomLeft);
      indices.push_back(bottomRight);
    }

  // ---- Skirts (optional, unchanged but will respect missing vertices) ----
  if (add_skirt)
  {
    auto add_skirt_edge = [&](auto getIndex, int count)
    {
      for (int k = 0; k < count - 1; ++k)
      {
        int top0 = vertex_map[getIndex(k)];
        int top1 = vertex_map[getIndex(k + 1)];
        if (top0 < 0 || top1 < 0)
          continue; // skip missing vertices

        int baseIdx = static_cast<int>(vertices.size());

        // Bottom skirt vertices (lower in Y)
        glm::vec3 bottomPos0 = vertices[top0].position;
        bottomPos0.y = add_level;
        glm::vec3 bottomPos1 = vertices[top1].position;
        bottomPos1.y = add_level;

        vertices.emplace_back(bottomPos0, glm::vec3(0, 0, 0), vertices[top0].uv);
        vertices.emplace_back(bottomPos1, glm::vec3(0, 0, 0), vertices[top1].uv);

        int bottom0 = baseIdx;
        int bottom1 = baseIdx + 1;

        indices.push_back(top0);
        indices.push_back(bottom0);
        indices.push_back(top1);

        indices.push_back(top1);
        indices.push_back(bottom0);
        indices.push_back(bottom1);
      }
    };

    add_skirt_edge([&](int j) { return j * width + 0; }, height);           // left edge
    add_skirt_edge([&](int j) { return j * width + (width - 1); }, height); // right edge
    add_skirt_edge([&](int i) { return i; }, width);                        // bottom edge
    add_skirt_edge([&](int i) { return (height - 1) * width + i; }, width); // top edge
  }

  // ---- Recalculate normals ----
  for (size_t i = 0; i < indices.size(); i += 3)
  {
    Vertex &v0 = vertices[indices[i + 0]];
    Vertex &v1 = vertices[indices[i + 1]];
    Vertex &v2 = vertices[indices[i + 2]];

    glm::vec3 edge1 = v1.position - v0.position;
    glm::vec3 edge2 = v2.position - v0.position;
    glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

    v0.normal += normal;
    v1.normal += normal;
    v2.normal += normal;
  }

  for (auto &v : vertices)
    v.normal = glm::normalize(v.normal);

  mesh.create(vertices, indices);
}

} // namespace qtr
