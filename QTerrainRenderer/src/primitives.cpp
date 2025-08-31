/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/mesh.hpp"

namespace qtr
{

void generate_cube(Mesh &mesh, float x, float y, float z, float lx, float ly, float lz)
{
  std::vector<Vertex> vertices;
  std::vector<uint>   indices;

  float hx = lx * 0.5f;
  float hy = ly * 0.5f;
  float hz = lz * 0.5f;

  glm::vec3 normals[6] = {
      {0.0f, 0.0f, 1.0f},  // Front
      {0.0f, 0.0f, -1.0f}, // Back
      {0.0f, 1.0f, 0.0f},  // Top
      {0.0f, -1.0f, 0.0f}, // Bottom
      {-1.0f, 0.0f, 0.0f}, // Left
      {1.0f, 0.0f, 0.0f}   // Right
  };

  glm::vec3 positions[8] = {
      {x - hx, y - hy, z - hz}, // 0: bottom-back-left
      {x + hx, y - hy, z - hz}, // 1: bottom-back-right
      {x + hx, y + hy, z - hz}, // 2: top-back-right
      {x - hx, y + hy, z - hz}, // 3: top-back-left
      {x - hx, y - hy, z + hz}, // 4: bottom-front-left
      {x + hx, y - hy, z + hz}, // 5: bottom-front-right
      {x + hx, y + hy, z + hz}, // 6: top-front-right
      {x - hx, y + hy, z + hz}  // 7: top-front-left
  };

  glm::vec2 uvs[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

  // Faces: each face has 4 vertices (quad)
  auto addFace = [&](int i0, int i1, int i2, int i3, const glm::vec3 &normal)
  {
    uint startIndex = vertices.size();
    vertices.emplace_back(positions[i0], normal, uvs[0]);
    vertices.emplace_back(positions[i1], normal, uvs[1]);
    vertices.emplace_back(positions[i2], normal, uvs[2]);
    vertices.emplace_back(positions[i3], normal, uvs[3]);

    // two triangles per face
    indices.push_back(startIndex);
    indices.push_back(startIndex + 1);
    indices.push_back(startIndex + 2);
    indices.push_back(startIndex);
    indices.push_back(startIndex + 2);
    indices.push_back(startIndex + 3);
  };

  // Build all 6 faces (front, back, top, bottom, left, right)
  addFace(4, 5, 6, 7, normals[0]); // Front
  addFace(1, 0, 3, 2, normals[1]); // Back
  addFace(3, 7, 6, 2, normals[2]); // Top
  addFace(0, 1, 5, 4, normals[3]); // Bottom
  addFace(0, 4, 7, 3, normals[4]); // Left
  addFace(5, 1, 2, 6, normals[5]); // Right

  mesh.create(vertices, indices);
}

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
                        float                     add_level)
{
  std::vector<Vertex> vertices;
  std::vector<uint>   indices;

  vertices.reserve(width * height);
  indices.reserve((width - 1) * (height - 1) * 6);

  float hx = lx * 0.5f;
  float hy = ly * 0.5f;

  float dx = lx / (width - 1);
  float dy = ly / (height - 1);

  // ---- Generate main surface vertices ----
  for (int j = 0; j < height; ++j)
  {
    for (int i = 0; i < width; ++i)
    {
      int idx = j * width + i;

      glm::vec3 position(x - hx + i * dx, z + data[idx] * lz, y - hy + j * dy);
      glm::vec3 normal(0.0f, 1.0f, 0.0f); // placeholder normal
      glm::vec2 uv(static_cast<float>(i) / (width - 1),
                   static_cast<float>(j) / (height - 1));

      vertices.emplace_back(position, normal, uv);
    }
  }

  // ---- Generate main surface indices ----
  for (int j = 0; j < height - 1; ++j)
  {
    for (int i = 0; i < width - 1; ++i)
    {
      int topLeft = j * width + i;
      int topRight = topLeft + 1;
      int bottomLeft = (j + 1) * width + i;
      int bottomRight = bottomLeft + 1;

      indices.push_back(topLeft);
      indices.push_back(bottomLeft);
      indices.push_back(topRight);

      indices.push_back(topRight);
      indices.push_back(bottomLeft);
      indices.push_back(bottomRight);
    }
  }

  // ---- Add skirts ----
  if (add_skirt)
  {
    auto add_skirt_edge = [&](auto getIndex, int count)
    {
      for (int k = 0; k < count - 1; ++k)
      {
        int baseIdx = static_cast<int>(vertices.size());

        // Original top vertex
        int top0 = getIndex(k);
        int top1 = getIndex(k + 1);

        // Bottom skirt vertices
        glm::vec3 bottomPos0 = vertices[top0].position;
        bottomPos0.y = add_level;
        glm::vec3 bottomPos1 = vertices[top1].position;
        bottomPos1.y = add_level;

        vertices.emplace_back(bottomPos0, glm::vec3(0, 0, 0), vertices[top0].uv);
        vertices.emplace_back(bottomPos1, glm::vec3(0, 0, 0), vertices[top1].uv);

        int bottom0 = baseIdx;
        int bottom1 = baseIdx + 1;

        // Two triangles per quad
        indices.push_back(top0);
        indices.push_back(bottom0);
        indices.push_back(top1);

        indices.push_back(top1);
        indices.push_back(bottom0);
        indices.push_back(bottom1);
      }
    };

    // Left edge (x-min)
    add_skirt_edge([&](int j) { return j * width + 0; }, height);

    // Right edge (x-max)
    add_skirt_edge([&](int j) { return j * width + (width - 1); }, height);

    // Bottom edge (y-min)
    add_skirt_edge([&](int i) { return i; }, width);

    // Top edge (y-max)
    add_skirt_edge([&](int i) { return (height - 1) * width + i; }, width);
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

void generate_plane(Mesh &mesh, float x, float y, float z, float lx, float ly)
{
  std::vector<Vertex> vertices;
  std::vector<uint>   indices;

  float hx = lx * 0.5f; // half size along X
  float hz = ly * 0.5f; // half size along Z

  glm::vec3 normal(0.0f, 1.0f, 0.0f); // up direction

  // 4 vertices (counter-clockwise)
  vertices = {
      {glm::vec3(x - hx, y, z - hz), normal, glm::vec2(0.0f, 0.0f)}, // bottom-left
      {glm::vec3(x + hx, y, z - hz), normal, glm::vec2(1.0f, 0.0f)}, // bottom-right
      {glm::vec3(x + hx, y, z + hz), normal, glm::vec2(1.0f, 1.0f)}, // top-right
      {glm::vec3(x - hx, y, z + hz), normal, glm::vec2(0.0f, 1.0f)}  // top-left
  };

  // indices for two triangles
  indices = {0, 1, 2, 2, 3, 0};

  mesh.create(vertices, indices);
}

} // namespace qtr
