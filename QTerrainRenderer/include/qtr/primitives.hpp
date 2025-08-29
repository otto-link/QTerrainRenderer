/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include "qtr/mesh.hpp"

namespace qtr
{

void generate_cube(Mesh &mesh, float x, float y, float z, float lx, float ly, float lz);
void generate_plane(Mesh &mesh, float x, float y, float z, float lx, float ly);

} // namespace qtr