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

uniform mat4 view;
uniform mat4 projection;
uniform vec2 screen_size;
uniform vec3 light_pos;
uniform vec3 camera_pos;
uniform vec3 view_pos;
uniform vec3 base_color;    
uniform float shininess;    
uniform float spec_strength; 
uniform bool bypass_shadow_map;
uniform float shadow_strength;
uniform bool add_ambiant_occlusion;
uniform float ambiant_occlusion_strength;
uniform int ambiant_occlusion_radius;
uniform bool use_texture_albedo;
uniform float gamma_correction;
uniform bool apply_tonemap;

uniform sampler2D texture_albedo; 
uniform sampler2D texture_hmap;
uniform sampler2D texture_shadow_map; 

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
        float closest_depth = texture(texture_shadow_map, proj_coords.xy).r;
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
        vec2 texel_size = 1.0 / textureSize(texture_shadow_map, 0);
        
        float sum = 0;
        int ir = 2;
        for(int x = -ir; x <= ir; ++x)
            for(int y = -ir; y <= ir; ++y)
            {
                float pcf_depth = texture(texture_shadow_map, proj_coords.xy + vec2(x, y) * texel_size).r; 
                float weight = 1.0 - length(vec2(x, y)) / (ir + 1);
                shadow += weight * (current_depth - bias > pcf_depth ? 1.0 : 0.0);
                sum += weight;
            }    
        shadow /= sum;

        return shadow;
    }
}

// Compute normalized ray direction for current pixel in world space
vec3 compute_ray_direction(vec2 frag_coord)
{
    // convert pixel to Normalized Device Coordinates (NDC) [-1, 1]
    vec2 ndc = (frag_coord / screen_size) * 2.0 - 1.0;
    ndc.y = -ndc.y; // Flip Y (OpenGL screen space vs texture space)

    // transform from clip space to eye space
    vec4 ray_clip = vec4(ndc, -1.0, 1.0);           // z = -1 for forward direction
    vec4 ray_eye = inverse(projection) * ray_clip;
    ray_eye = vec4(ray_eye.xy, -1.0, 0.0);           // direction in eye space

    // transform from eye space to world space
    vec3 ray_world = normalize((inverse(view) * ray_eye).xyz);

    return ray_world;
}

// https://iquilezles.org/articles/intersectors
vec2 boxIntersection(in vec3 ro, in vec3 rd, vec3 boxSize, out vec3 outNormal) 
{
    // ro: ray origin (e.g. camera position)
    // rd: ray direction (e.g. direction from camera to current pixel)

    vec3 m = 1.0/rd; // can precompute if traversing a set of aligned boxes
    vec3 n = m*ro;   // can precompute if traversing a set of aligned boxes
    vec3 k = abs(m)*boxSize;
    vec3 t1 = -n - k;
    vec3 t2 = -n + k;
    float tN = max( max( t1.x, t1.y ), t1.z );
    float tF = min( min( t2.x, t2.y ), t2.z );
    if( tN>tF || tF<0.0) return vec2(-1.0); // no intersection
    outNormal = -sign(rd)*step(t1.yzx,t1.xyz)*step(t1.zxy,t1.xyz);

    // returns entry / exit distances from ro
    return vec2( tN, tF );
}

// g controls forward/backward scattering: 0 = isotropic, 0.6 = forward-scattering
float phase_mie(float cos_theta, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cos_theta, 1.5);
}

float phase_rayleigh(float cos_theta)
{
    return 3.0 / (16.0 * 3.1415926535) * (1.0 + cos_theta * cos_theta);
}

float compute_AO(vec2 uv, sampler2D hmap, int radius, float strength)
{
    vec2 texel_size = 1.0 / textureSize(hmap, 0);
    float h = texture(hmap, uv).r;
    float occ = 0.0;
    int count = 0;
    
    for (int x = -radius; x <= radius; x++)
        for (int y = -radius; y <= radius; y++)
        {
            if (x == 0 && y == 0) continue;
            
            float neighbor = texture(hmap, uv + vec2(x, y) * texel_size).r;
            if (neighbor > h) occ += neighbor - h;
            count++;
        }
   
    occ = occ / float(count) * 2.0; // in [0 ,1]
    occ *= strength;
    occ = clamp(1.0 - occ, 0.0, 1.0);
    
    return occ;
}

// https://www.shadertoy.com/view/WdjSW3
vec3 tonemap_ACES(vec3 x)
{
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

void main()
{
    vec3 color;
    if (use_texture_albedo)
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

    // apply shadow
    if (true)
    {
        float diff_m = min(diff, 1.0 - shadow);
        diff_m = 1.0 - shadow_strength + shadow_strength * smoothstep(1.0 - shadow_strength, 1.0, diff_m);

        vec3 diffuse = color * diff_m;
        vec3 specular = spec_strength * spec * vec3(1.0);
        vec3 ambient = 0.2 * color;

        if (add_ambiant_occlusion)
        {
            float ao = compute_AO(frag_uv, texture_hmap, ambiant_occlusion_radius, ambiant_occlusion_strength);
            ambient *= ao;
        }

        vec3 result = ambient + diffuse + specular;

        frag_color = vec4(result, 1.0);
    }
    
    // volumetric fog
    if (false)
    {
        vec3 box_normal;
        vec3 ray_origin = camera_pos;
        vec3 ray_dir = compute_ray_direction(gl_FragCoord.xy);
        vec2 box = boxIntersection(camera_pos, ray_dir, vec3(4.0, 0.2, 4.0), box_normal);

        // frag_color = vec4(normalize(ray_dir) * 0.5 + 0.5, 1.0);
        // return;

            // ---- 2. Setup fog integration ----
        float step_size = 1.0;            // step length along the ray
        float max_distance = 100.0;
        int steps = int(max_distance / step_size);
        float transmittance = 1.0;        // starts fully transparent
        vec3 fog_color = vec3(0.0);       // accumulated fog
        float fog_height = 0.1;
        float fog_density = 0.1;
        vec3 light_color = vec3(0.0, 0.0, 1.0);

        for (int i = 0; i < steps; i++)
        {
            float t = float(i) * step_size;
            vec3 p = ray_origin + ray_dir * t;         // point along the ray

            if (p.y > 0.0)
            {
                // ---- 3. Density based on height ----
                float height_factor = clamp(exp(-p.y * fog_height), 0.0, 1.0);
                float density = fog_density * height_factor;

                // ---- 4. Light scattering ----
                float scatter = max(dot(ray_dir, light_dir), 0.0);

                // ---- 5. Attenuation and accumulation ----
                float absorption = exp(-density * step_size);
                transmittance *= absorption;

                float cos_theta = dot(ray_dir, light_dir);
                float scatter_phase = 
                    phase_rayleigh(cos_theta) * 0.5 + // mix rayleigh for softness
                    phase_mie(cos_theta, 0.2) * 0.5;  // and mie for forward bias

                fog_color += transmittance * density * scatter_phase * light_color * step_size;
            }
        }

        frag_color.xyz = frag_color.xyz * transmittance + fog_color;

        // frag_color.x = ray_length;
    }

    if (apply_tonemap)
        frag_color = vec4(tonemap_ACES(frag_color.xyz), 1.0);
}
)"";

} // namespace qtr