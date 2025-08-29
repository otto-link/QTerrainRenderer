/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>

namespace qtr
{

// small wrapper for convenience and safety
class Shader : protected QOpenGLFunctions_3_3_Core
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

static const std::string diffuse_basic_vertex = R""(
#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 frag_normal;
out vec3 frag_pos;
out vec2 frag_uv;

void main()
{
    frag_pos = vec3(model * vec4(pos, 1.0));
    frag_normal = mat3(transpose(inverse(model))) * normal;
    frag_uv = uv;

    gl_Position = projection * view * vec4(frag_pos, 1.0);

    // gl_Position = vec4(pos, 1.0);
}
)"";

static const std::string diffuse_basic_frag = R""(
#version 330 core

in vec3 frag_normal;
in vec3 frag_pos;
in vec2 frag_uv;

out vec4 frag_color;

uniform vec3 color;
uniform vec3 light_dir;

void main()
{
    // Normalize the interpolated normal
    vec3 norm = normalize(frag_normal);
    vec3 light = normalize(light_dir);

    // Basic diffuse shading
    float diff = max(dot(norm, light), 0.0);
    vec3 base_color = color * (0.2 + 0.8 * diff);

    frag_color = vec4(base_color, 1.0);
    // frag_color = vec4(1.0, 0.0, 0.0, 1.0);
}
)"";

static const std::string diffuse_phong_frag = R""(
#version 330 core

in vec3 frag_normal;
in vec3 frag_pos;
in vec2 frag_uv;

out vec4 frag_color;

uniform vec3 color;        // Base color of the object
uniform vec3 light_dir;    // Direction *towards* the light
uniform vec3 view_pos;     // Camera position in world space
uniform float shininess;   // Controls specular sharpness
uniform float spec_strength; // Controls specular intensity

void main()
{
    vec3 norm = normalize(frag_normal);
    vec3 light = normalize(light_dir);
    vec3 view_dir = normalize(view_pos - frag_pos);

    // --- Diffuse ---
    float diff = max(dot(norm, light), 0.0);
    vec3 diffuse = color * diff;

    // --- Specular (Phong) ---
    vec3 reflect_dir = reflect(-light, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), shininess);
    vec3 specular = spec_strength * spec * vec3(1.0);

    // --- Ambient ---
    vec3 ambient = 0.2 * color;

    // Combine results
    vec3 result = ambient + diffuse + specular;
    frag_color = vec4(result, 1.0);
}
)"";

static const std::string diffuse_blinn_phong_frag = R""(
#version 330 core

in vec3 frag_normal;
in vec3 frag_pos;
in vec2 frag_uv;

out vec4 frag_color;

uniform vec3 color;        // Base color of the object
uniform vec3 light_dir;    // Direction *towards* the light
uniform vec3 view_pos;     // Camera position in world space
uniform float shininess;   // Controls specular sharpness
uniform float spec_strength; // Controls specular intensity

void main()
{
    vec3 norm = normalize(frag_normal);
    vec3 light = normalize(light_dir);
    vec3 view_dir = normalize(view_pos - frag_pos);

    // --- Diffuse ---
    float diff = max(dot(norm, light), 0.0);
    vec3 diffuse = color * diff;

    // --- Specular (Blinn-Phong) ---
    vec3 halfway_dir = normalize(light + view_dir);
    float spec = pow(max(dot(norm, halfway_dir), 0.0), shininess);
    vec3 specular = spec_strength * spec * vec3(1.0);

    // --- Ambient ---
    vec3 ambient = 0.2 * color;

    // Combine results
    vec3 result = ambient + diffuse + specular;
    frag_color = vec4(result, 1.0);
}
)"";

} // namespace qtr