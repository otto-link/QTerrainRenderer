/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

namespace qtr
{

// small wrapper for convenience and safety
class Shader : protected QOpenGLFunctions
{
public:
  Shader() = default;
  ~Shader();

  bool from_code(const std::string &vertex_code, const std::string &fragment_code);
  bool from_file(const std::string &vertex_path, const std::string &fragment_path);

  QOpenGLShaderProgram       *get();
  const QOpenGLShaderProgram *get() const;

private:
  void destroy();

  std::unique_ptr<QOpenGLShaderProgram> sp_program;
};

} // namespace qtr