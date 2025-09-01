R""(
/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 light_space_matrix;

out vec3 frag_pos;
out vec3 frag_normal;
out vec2 frag_uv;
out vec4 frag_pos_light_space;

void main()
{
  frag_pos = vec3(model * vec4(pos, 1.0));
  frag_normal = mat3(transpose(inverse(model))) * normal;
  frag_uv = uv;

  frag_pos_light_space = light_space_matrix * vec4(frag_pos, 1.0);
  gl_Position = projection * view * vec4(frag_pos, 1.0);
}
)""
