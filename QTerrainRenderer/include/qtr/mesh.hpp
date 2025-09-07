/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>
#include <vector>

#include <QOpenGLFunctions_3_3_Core>

#include <glm/glm.hpp>

namespace qtr
{

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

class Mesh : protected QOpenGLFunctions_3_3_Core
{
public:
  Mesh();
  ~Mesh();

  void create(const std::vector<Vertex> &vertices, const std::vector<uint> &indices = {});
  void destroy();
  void draw();
  bool is_active() const;
  void update_vertices(const std::vector<Vertex> &vertices);

  GLuint vao = 0;
  GLuint vbo = 0;
  GLuint ebo = 0;
  size_t vertex_count = 0;
  size_t index_count = 0;
  bool   has_indices;
};

} // namespace qtr