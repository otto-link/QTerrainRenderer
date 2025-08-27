/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <string>

#include <QOpenGLFunctions>

namespace qtr
{

class Texture : protected QOpenGLFunctions
{
public:
  Texture();
  ~Texture();

  bool from_image_8bit_grayscale(const std::vector<uint8_t> &img, int new_width);
  bool from_image_8bit_rgba(const std::vector<uint8_t> &img, int new_width);
  bool from_image_16bit_grayscale(const std::vector<uint16_t> &img, int new_width);

  // bool load_from_file(const std::string &path, bool flip_y = true)
  // {
  //     initializeOpenGLFunctions();
  //     destroy();

  //     // QImage image(QString::fromStdString(path));
  //     // if (image.isNull()) return false;

  //     // if (flip_y)
  //     //     image = image.mirrored(false, true);

  //     // QImage glImage = image.convertToFormat(QImage::Format_RGBA8888);
  //     // width = glImage.width();
  //     // height = glImage.height();

  //     // glGenTextures(1, &id);
  //     // glBindTexture(GL_TEXTURE_2D, id);

  //     // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
  //     //              0, GL_RGBA, GL_UNSIGNED_BYTE, glImage.bits());

  //     // glGenerateMipmap(GL_TEXTURE_2D);

  //     // // Set default parameters
  //     // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  //     // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  //     // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
  //     GL_LINEAR_MIPMAP_LINEAR);
  //     // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  //     // glBindTexture(GL_TEXTURE_2D, 0);
  //     return true;
  // }

  GLuint get_id() const;
  int    get_width() const;
  int    get_height() const;

  void bind(int unit = 0);
  void unbind();
  void destroy();

private:
  GLuint id;
  int    width;
  int    height;
};
} // namespace qtr