/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <string>

#include <GL/gl.h>

#include "qtr/logger.hpp"

namespace qtr
{

inline std::string gl_error_to_string(GLenum error)
{
  switch (error)
  {
  case GL_NO_ERROR:
    return "No error";
  case GL_INVALID_ENUM:
    return "Invalid enum";
  case GL_INVALID_VALUE:
    return "Invalid value";
  case GL_INVALID_OPERATION:
    return "Invalid operation";
  case GL_INVALID_FRAMEBUFFER_OPERATION:
    return "Invalid framebuffer operation";
  case GL_OUT_OF_MEMORY:
    return "Out of memory";
  default:
    return "Unknown error";
  }
}

inline void check_gl_error(const std::string &label = "")
{
  GLenum error;
  while ((error = glGetError()) != GL_NO_ERROR)
    QTR_LOG->error("[OpenGL Error] {}: {}", label, gl_error_to_string(error));
}

} // namespace qtr