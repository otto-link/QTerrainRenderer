/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <string>
#include <vector>

namespace qtr
{

std::vector<uint8_t> load_png_as_8bit_rgba(const std::string &path,
                                           int               &width,
                                           int               &height);

std::vector<float> load_png_as_grayscale(const std::string &path,
                                         int               &width,
                                         int               &height);

} // namespace qtr