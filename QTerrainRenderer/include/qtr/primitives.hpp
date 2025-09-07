/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include "qtr/mesh.hpp"

namespace qtr
{
/* OpenGL convention for all coordinates
   X → horizontal span (lx)
   Y → height (ly, modified by the heightmap data)
   Z → depth span (lz) */

void generate_cube(Mesh &mesh, float x, float y, float z, float lx, float ly, float lz);

void generate_heightmap(Mesh                     &mesh,
                        const std::vector<float> &data,
                        int                       width,
                        int                       height,
                        float                     x,
                        float                     y,
                        float                     z,
                        float                     lx,
                        float                     ly,
                        float                     lz,
                        bool                      add_skirt = false,
                        float                     add_level = 0.f,
                        float                     exclude_below = -FLT_MAX);

void generate_path(Mesh &mesh, const std::vector<glm::vec3> &points, float width);

void generate_plane(Mesh &mesh, float x, float y, float z, float lx, float ly);

void generate_downward_triangles(Mesh                         &mesh,
                                 const std::vector<glm::vec3> &points,
                                 float                         height_offset = 0.1f,
                                 float                         radius = 0.01f);

// center of one edge -> center of opposite edge
void generate_rectangle(Mesh            &mesh,
                        const glm::vec3 &p1,
                        const glm::vec3 &p2,
                        float            height);

} // namespace qtr