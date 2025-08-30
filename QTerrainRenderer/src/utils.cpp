/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <QImage>

#include "qtr/logger.hpp"

namespace qtr
{

std::vector<float> load_png_as_grayscale(const std::string &path, int &width, int &height)
{
  QImage img(path.c_str());
  if (img.isNull())
  {
    throw std::runtime_error("Failed to load image: " + path);
  }

  // Convert to 16-bit grayscale
  if (img.format() != QImage::Format_Grayscale16)
    img = img.convertToFormat(QImage::Format_Grayscale16);

  width = img.width();
  height = img.height();

  std::vector<float> data(width * height);
  const uint16_t    *src = reinterpret_cast<const uint16_t *>(img.constBits());

  for (int i = 0; i < width * height; ++i)
    data[i] = static_cast<float>(src[i]) / 65535.0f; // Normalize

  return data;
}

} // namespace qtr
