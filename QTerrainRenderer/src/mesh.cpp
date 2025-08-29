/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/mesh.hpp"
#include "qtr/logger.hpp"

namespace qtr
{

Vertex::Vertex(const glm::vec3 &pos, const glm::vec3 &norm, const glm::vec2 &tex_uv)
{
  this->position = {pos.x, pos.y, pos.z};
  this->normal = {norm.x, norm.y, norm.z};
  this->uv = {tex_uv.x, tex_uv.y};
}

Mesh::Mesh() {}

Mesh::~Mesh() { this->destroy(); }

void Mesh::create(const std::vector<Vertex> &vertices, const std::vector<uint> &indices)
{
  QOpenGLFunctions_3_3_Core::initializeOpenGLFunctions();
  this->destroy();

  this->vertex_count = vertices.size();
  this->index_count = indices.size();
  this->has_indices = !indices.empty();

  // Create VAO
  glGenVertexArrays(1, &this->vao);
  glBindVertexArray(this->vao);

  // Create VBO
  glGenBuffers(1, &this->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
  glBufferData(GL_ARRAY_BUFFER,
               vertices.size() * sizeof(Vertex),
               vertices.data(),
               GL_DYNAMIC_DRAW);

  // Create EBO
  if (this->has_indices)
  {
    glGenBuffers(1, &this->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indices.size() * sizeof(uint),
                 indices.data(),
                 GL_STATIC_DRAW);
  }

  // Vertex attributes
  GLsizei stride = sizeof(Vertex);
  glEnableVertexAttribArray(0); // position
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

  glEnableVertexAttribArray(1); // normal
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));

  glEnableVertexAttribArray(2); // textcoord
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));

  glBindVertexArray(0);
}

void Mesh::update_vertices(const std::vector<Vertex> &vertices)
{
  if (!this->vbo)
    return;
  glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::draw()
{
  glBindVertexArray(this->vao);
  if (this->has_indices)
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(this->index_count),
                   GL_UNSIGNED_INT,
                   nullptr);
  else
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(this->vertex_count));
  glBindVertexArray(0);
}

void Mesh::destroy()
{
  if (this->vbo)
    glDeleteBuffers(1, &this->vbo);
  if (this->ebo)
    glDeleteBuffers(1, &this->ebo);
  if (this->vao)
    glDeleteVertexArrays(1, &this->vao);

  this->vbo = 0;
  this->ebo = 0;
  this->vao = 0;

  this->vertex_count = 0;
  this->index_count = 0;
  this->has_indices = false;
}

} // namespace qtr
