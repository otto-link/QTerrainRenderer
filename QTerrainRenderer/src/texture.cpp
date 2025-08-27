/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
 License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/texture.hpp"

namespace qtr
{

Texture::Texture() : id(0), width(0), height(0) {}

Texture::~Texture() { this->destroy(); }

void Texture::bind(int unit)
{
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, this->id);
}

void Texture::destroy()
{
  if (this->id != 0)
  {
    glDeleteTextures(1, &this->id);
    this->id = 0;
  }
}

bool Texture::from_image_8bit_grayscale(const std::vector<uint8_t> &img, int new_width)
{
  QOpenGLFunctions::initializeOpenGLFunctions();
  this->destroy();

  this->width = new_width;
  this->height = img.size() / this->width;

  glGenTextures(1, &this->id);
  glBindTexture(GL_TEXTURE_2D, this->id);

  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_R8, // explicitly specify 8-bit internal format (for Windows)
               this->width,
               this->height,
               0,
               GL_RED,
               GL_UNSIGNED_BYTE, // uint8_t
               img.data());

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, 0);

  return true;
}

bool Texture::from_image_8bit_rgba(const std::vector<uint8_t> &img, int new_width)
{
  QOpenGLFunctions::initializeOpenGLFunctions();
  this->destroy();

  this->width = new_width;
  this->height = img.size() / 4 / this->width;

  glGenTextures(1, &this->id);
  glBindTexture(GL_TEXTURE_2D, this->id);

  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               this->width,
               this->height,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE, // uint8_t
               img.data());

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, 0);

  return true;
}

bool Texture::from_image_16bit_grayscale(const std::vector<uint16_t> &img, int new_width)
{
  QOpenGLFunctions::initializeOpenGLFunctions();
  this->destroy();

  this->width = new_width;
  this->height = img.size() / this->width;

  glGenTextures(1, &this->id);
  glBindTexture(GL_TEXTURE_2D, this->id);

  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_R16, // explicitly specify 16-bit internal format (for Windows)
               this->width,
               this->height,
               0,
               GL_RED,
               GL_UNSIGNED_SHORT, // uint16_t
               img.data());

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, 0);

  return true;
}

GLuint Texture::get_id() const { return this->id; }

int Texture::get_width() const { return this->width; }

int Texture::get_height() const { return this->height; }

void Texture::unbind() { glBindTexture(GL_TEXTURE_2D, 0); }

} // namespace qtr
