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

static const std::string shadow_map_depth_pass_vertex = R""(
#version 330 core

layout(location = 0) in vec3 pos;

uniform mat4 light_space_matrix;
uniform mat4 model;

void main()
{
    gl_Position = light_space_matrix * model * vec4(pos, 1.0);
}

)"";

static const std::string shadow_map_depth_pass_frag = R""(
#version 330 core

void main()
{
    // depth only, no output needed
}
)"";

static const std::string shadow_map_lit_pass_vertex = R""(
#version 330 core

layout (location = 0) in vec3 pos;    
layout (location = 1) in vec3 normal; 
layout (location = 2) in vec2 uv;   

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 light_space_matrix; // From first pass

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

)"";

static const std::string shadow_map_lit_pass_frag = R""(
#version 330 core

in vec3 frag_pos;
in vec3 frag_normal;
in vec2 frag_uv;
in vec4 frag_pos_light_space;

out vec4 frag_color;

uniform vec3 light_pos;
uniform vec3 view_pos;
uniform vec3 base_color;    
uniform float shininess;    
uniform float spec_strength; 
uniform bool bypass_shadow_map;
uniform float shadow_strength;
uniform bool use_texture;
uniform float gamma_correction;

uniform sampler2D shadow_map; // depth texture
uniform sampler2D texture_albedo; // albedo texture

float calculate_shadow(vec4 frag_pos_light_space, vec3 light_dir, vec3 frag_normal)
{
    // Perspective divide
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5; // [0,1] range

    // Check if outside light frustum
    if (proj_coords.z > 1.0)
        return 0.0;

    if (false)
    {
        float closest_depth = texture(shadow_map, proj_coords.xy).r;
        float current_depth = proj_coords.z;

        // Bias to prevent shadow acne
        float bias = max(0.001 * (1.0 - dot(frag_normal, light_dir)), 0.0001);

        // Simple shadow (0 or 1)
        return current_depth - bias > closest_depth ? 1.0 : 0.0;
    }

    if (true) // PCF
    {
        float current_depth = proj_coords.z;
     
        // locally tune bias
        float bias_min = 0.0005f;
        float bias_max = 0.001f;
        float bias_t = clamp(dot(frag_normal, light_dir), 0.0, 1.0); // in [0, 1]
        float bias = mix(bias_max, bias_min, bias_t);

        float shadow = 0.0;
        vec2 texel_size = 1.0 / textureSize(shadow_map, 0);
        
        // int count = 0;
        // for(int x = -1; x <= 1; ++x)
        //     for(int y = -1; y <= 1; ++y)
        //     {
        //         float pcf_depth = texture(shadow_map, proj_coords.xy + vec2(x, y) * texel_size).r; 
        //         shadow += current_depth - bias > pcf_depth ? 1.0 : 0.0;
        //         count++;
        //     }    
        // shadow /= count;
        
        float sum = 0;
        int ir = 2;
        for(int x = -ir; x <= ir; ++x)
            for(int y = -ir; y <= ir; ++y)
            {
                float pcf_depth = texture(shadow_map, proj_coords.xy + vec2(x, y) * texel_size).r; 
                float weight = 1.0 - length(vec2(x, y)) / (ir + 1);
                shadow += weight * (current_depth - bias > pcf_depth ? 1.0 : 0.0);
                sum += weight;
            }    
        shadow /= sum;

        return shadow;
    }

}

void main()
{
    vec3 color;
    if (use_texture)
        color = texture(texture_albedo, frag_uv).xyz; // TODO alpha channel
    else
        color = base_color;

    color.x = pow(color.x, 1.0 / gamma_correction);
    color.y = pow(color.y, 1.0 / gamma_correction);
    color.z = pow(color.z, 1.0 / gamma_correction);

    // frag_color = vec4(color, 1.0);
    // return;

    vec3 norm = normalize(frag_normal);
    vec3 light_dir = normalize(light_pos - frag_pos);
    vec3 view_dir = normalize(view_pos - frag_pos);

    // Diffuse
    float diff = max(dot(norm, light_dir), 0.0);

    // Specular
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = spec_strength * pow(max(dot(view_dir, reflect_dir), 0.0), shininess);

    // Shadow factor
    float shadow = 0.0;
    if (!bypass_shadow_map)
        shadow = calculate_shadow(frag_pos_light_space, light_dir, frag_normal);

    if (false)
    {
        // vec3 lighting = (0.2 * color) + ((1.0 - shadow) * (diff * color + 0.5 * spec * vec3(1.0)));

        // vec3 shadow_color = vec3(0.4, 0.4, 0.5); // bluish soft shadow tint (0-1 range)
        // vec3 shadow_color = vec3(0.5, 0.3, 0.2); // warm sunset shadows
        vec3 shadow_color = vec3(0., 0., 0.); // dark night

        float shadow_intensity = 1;            // 0 = no shadow, 1 = full shadow

        vec3 base_light = diff * color + 0.5 * spec * vec3(1.0);
        vec3 shadow_mix = mix(base_light, base_light * shadow_color, shadow * shadow_intensity);

        vec3 lighting = (0.2 * color) + shadow_mix; // ambiant light + shadow

        frag_color = vec4(lighting, 1.0);
    }

    if (true)
    {
        float diff_m = min(diff, 1.0 - shadow);
        diff_m = 1.0 - shadow_strength + shadow_strength * smoothstep(1.0 - shadow_strength, 1.0, diff_m);

        vec3 diffuse = color * diff_m;
        vec3 specular = spec_strength * spec * vec3(1.0);
        vec3 ambient = 0.2 * color;
        vec3 result = ambient + diffuse + specular;

        frag_color = vec4(result, 1.0);
    }
    
}
)"";

} // namespace qtr