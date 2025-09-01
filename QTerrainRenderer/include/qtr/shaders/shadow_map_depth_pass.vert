R""(
/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#version 330 core

layout(location = 0) in vec3 pos;

uniform mat4 light_space_matrix;
uniform mat4 model;

void main() { gl_Position = light_space_matrix * model * vec4(pos, 1.0); }
)""
