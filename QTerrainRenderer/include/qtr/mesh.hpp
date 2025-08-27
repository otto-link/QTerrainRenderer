/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>
#include <vector>

#include <QOpenGLFunctions_3_3_Core>

namespace qtr
{

struct Vertex
{
  float position[3];
  float normal[3];
  float texcoord[2];
};

class Mesh : protected QOpenGLFunctions_3_3_Core
{
public:
  Mesh();
  ~Mesh();

  void create(const std::vector<Vertex>       &vertices,
              const std::vector<unsigned int> &indices = {});
  void destroy();
  void draw();
  void update_vertices(const std::vector<Vertex> &vertices);

private:
  GLuint vao = 0;
  GLuint vbo = 0;
  GLuint ebo = 0;
  size_t vertex_count = 0;
  size_t index_count = 0;
  bool   has_indices;
};

} // namespace qtr