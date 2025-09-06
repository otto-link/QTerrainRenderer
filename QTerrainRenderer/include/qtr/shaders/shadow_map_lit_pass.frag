R""(
/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#version 330 core

in vec3 frag_pos;
in vec3 frag_normal;
in vec2 frag_uv;
in vec4 frag_pos_light_space;

out vec4 frag_color;

uniform vec2  screen_size;
uniform float time;
uniform float near_plane;
uniform float far_plane;
uniform float scale_h;

uniform mat4  view;
uniform mat4  projection;
uniform vec3  light_pos;
uniform mat4  light_space_matrix;
uniform vec3  camera_pos;
uniform vec3  view_pos;
uniform vec3  base_color;
uniform float shininess;
uniform float spec_strength;
uniform bool  bypass_shadow_map;
uniform float shadow_strength;
uniform bool  add_ambiant_occlusion;
uniform float ambiant_occlusion_strength;
uniform int   ambiant_occlusion_radius;
uniform bool  use_texture_albedo;

uniform bool  use_water_colors;
uniform vec3  color_shallow_water;
uniform vec3  color_deep_water;
uniform float water_color_depth;
uniform bool  add_water_foam;
uniform vec3  foam_color;
uniform float foam_depth;
uniform bool  add_water_waves;
uniform float angle_spread_ratio;
uniform float waves_alpha;
uniform float waves_kw;
uniform float waves_amplitude;
uniform float waves_normal_amplitude;
uniform float waves_speed;

uniform bool add_fog;

uniform bool add_atmospheric_scattering;

uniform float gamma_correction;
uniform bool  apply_tonemap;

uniform sampler2D texture_albedo;
uniform sampler2D texture_hmap;
uniform sampler2D texture_shadow_map;
uniform sampler2D texture_depth;

float calculate_shadow(vec4 frag_pos_light_space,
                       vec3 light_dir,
                       vec3 frag_normal,
                       bool use_pcf)
{
  // Perspective divide
  vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
  proj_coords = proj_coords * 0.5 + 0.5; // [0,1] range

  // Check if outside light frustum
  if (proj_coords.z > 1.0)
    return 0.0;

  if (!use_pcf)
  {
    float closest_depth = texture(texture_shadow_map, proj_coords.xy).r;
    float current_depth = proj_coords.z;

    // Bias to prevent shadow acne
    float bias = max(0.001 * (1.0 - dot(frag_normal, light_dir)), 0.0001);

    // Simple shadow (0 or 1)
    return current_depth - bias > closest_depth ? 1.0 : 0.0;
  }
  else // PCF
  {
    float current_depth = proj_coords.z;

    // locally tune bias
    float bias_min = 0.0005f;
    float bias_max = 0.001f;
    float bias_t = clamp(dot(frag_normal, light_dir), 0.0, 1.0); // in [0, 1]
    float bias = mix(bias_max, bias_min, bias_t);

    float shadow = 0.0;
    vec2  texel_size = 1.0 / textureSize(texture_shadow_map, 0);

    float sum = 0;
    int   ir = 2;
    for (int x = -ir; x <= ir; ++x)
      for (int y = -ir; y <= ir; ++y)
      {
        float pcf_depth = texture(texture_shadow_map,
                                  proj_coords.xy + vec2(x, y) * texel_size)
                              .r;
        float weight = 1.0 - length(vec2(x, y)) / (ir + 1);
        shadow += weight * (current_depth - bias > pcf_depth ? 1.0 : 0.0);
        sum += weight;
      }
    shadow /= sum;

    return shadow;
  }
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
  vec2  texel_size = 1.0 / textureSize(hmap, 0);
  float h = texture(hmap, uv).r;
  float occ = 0.0;
  int   count = 0;

  for (int x = -radius; x <= radius; x++)
    for (int y = -radius; y <= radius; y++)
    {
      if (x == 0 && y == 0)
        continue;

      float neighbor = texture(hmap, uv + vec2(x, y) * texel_size).r;
      if (neighbor > h)
        occ += neighbor - h;
      count++;
    }

  occ = occ / float(count) * 2.0; // in [0 ,1]
  occ *= strength;
  occ = clamp(1.0 - occ, 0.0, 1.0);

  return occ;
}

// horizonn-based ambiant occlusion
float compute_hbao(vec2 uv, sampler2D hmap, float scale, int dir_count, int step_count)
{
  float h0 = texture(hmap, uv).r;
  float occ = 0.0;

  for (int d = 0; d < dir_count; d++)
  {
    float angle = 2.0 * 3.14159265 * float(d) / float(dir_count);
    vec2  dir = vec2(cos(angle), sin(angle));

    float horizon = -1e6;

    for (int s = 1; s <= step_count; s++)
    {
      vec2  suv = uv + dir * (float(s) / float(step_count)) * scale;
      float hs = texture(hmap, suv).r;
      float slope = (hs - h0) / float(s);
      horizon = max(horizon, slope);
    }

    occ += max(0.0, horizon);
  }

  return clamp(1.0 - occ / float(dir_count), 0.0, 1.0);
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

float sigmoid(float x, float width, float x0)
{
  float v = 1.f / (1.f + exp(-(x - x0) / width));
  return v;
}

float hash(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }

// ------------------------------------------------------------
// 2D -> 2D hash (deterministic, cheap, no trig)
// Adapted from Dave Hoskins' "hash without sine" style hashes.
// 'seed' lets you decorrelate multiple fields.
// ------------------------------------------------------------
vec2 hash22f(vec2 p, float seed)
{
  // fold the seed in; any small non-zero scaling works
  p += seed;

  // magic constants picked for good bit diffusion
  const vec3 k = vec3(0.1031, 0.11369, 0.13787);

  vec3 q = fract(vec3(p.x, p.y, p.x) * k);
  q += dot(q, q.yzx + 19.19);

  // return two reasonably independent components in [0,1)
  return fract(vec2(q.x * q.y, q.z * q.x));
}

// ------------------------------------------------------------
// Gabor-like wavelet sum (scalar output)
// p  : sample position (world or texture space)
// dir: preferred direction (need not be normalized; we normalize inside)
// angle_spread_ratio: how much directions vary per-cell around 'dir'
// fseed: randomization seed
// ------------------------------------------------------------
float gabor_wave_scalar(vec2 p, vec2 dir, float angle_spread_ratio, float fseed)
{
  vec2 ip = floor(p);
  vec2 fp = p - ip; // fractional part of p

  const float fr = 6.28318530718; // 2*pi
  const float fa = 4.0;           // gaussian falloff factor

  float av = 0.0;
  float at = 0.0;

  // 5x5 neighborhood
  for (int j = -2; j <= 2; ++j)
    for (int i = -2; i <= 2; ++i)
    {
      vec2 o = vec2(i, j);

      // jitter the feature point inside the cell
      vec2 h = hash22f(ip + o, fseed);
      vec2 r = fp - (o + h);

      // vary direction per-cell around 'dir'
      vec2 k = normalize(dir + angle_spread_ratio * (2.0 * hash22f(ip + o, fseed) - 1.0));

      float d = dot(r, r);
      float l = dot(r, k);
      float w = exp(-fa * d); // gaussian window
      float cs = cos(fr * l); // carrier

      av += w * cs;
      at += w;
    }

  return av / max(at, 1e-6); // safe normalize
}

float linearize_depth(float depth_sample)
{
  float z = depth_sample * 2.0 - 1.0;
  return (2.0 * near_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

float phase_hg(float cos_theta, float g)
{
  float denom = 1.0 + g * g - 2.0 * g * cos_theta;
  return (1.0 - g * g) / (4.0 * 3.14159265 * pow(denom, 1.5));
}

void main()
{
  vec3  color;
  float alpha = 1.0;
  vec3  normal = frag_normal;

  if (use_water_colors)
  {
    float h = texture(texture_hmap, frag_uv).r;
    float depth = frag_pos.y / 0.4 - h; // TODO fix scaling

    if (add_water_waves)
    {
      vec2 dir = vec2(cos(waves_alpha), sin(waves_alpha));

      // TODO use normal map
      float eps = 0.001;
      float dhdx = (texture(texture_hmap, frag_uv + vec2(eps, 0.0)).r - h) / eps;
      float dhdy = (texture(texture_hmap, frag_uv + vec2(0.0, eps)).r - h) / eps;
      vec2  dir_slope = normalize(vec2(dhdx, dhdy));

      float attenuation = exp(-depth / (2.0 * water_color_depth));
      dir = mix(dir, dir_slope, attenuation);

      // float waves_kw = 128.0;
      float fseed = 0.f;

      // change color only on the shore
      float gw = gabor_wave_scalar(waves_kw * frag_uv - waves_speed * time * dir,
                                   dir,
                                   angle_spread_ratio,
                                   fseed);
      depth += waves_amplitude * (0.5 * gw + 0.5) * attenuation;

      normal.xz += waves_normal_amplitude * gw * dir * (1.0 - attenuation);
    }

    if (depth > 0.f)
    {

      float transparency = exp(-depth / water_color_depth);
      color = mix(color_shallow_water, color_deep_water, 1.0 - transparency);
      alpha = 1.0 - transparency;

      // add foam
      if (add_water_foam)
      {
        // float foam_mask = exp(-depth / foam_depth);
        float foam_mask = 1.0 - sigmoid(depth, 0.5f * foam_depth, foam_depth);

        if (foam_mask > alpha)
        {
          color = mix(color, foam_color, foam_mask);
          alpha = pow(foam_mask, 0.7);
        }
      }
    }
    else
    {
      // ground above water
      alpha = 0.0;
    }
  }
  else
  {
    if (use_texture_albedo)
      color = texture(texture_albedo, frag_uv).xyz; // TODO alpha channel
    else
      color = base_color;
  }

  color.x = pow(color.x, 1.0 / gamma_correction);
  color.y = pow(color.y, 1.0 / gamma_correction);
  color.z = pow(color.z, 1.0 / gamma_correction);

  vec3 norm = normalize(normal);
  vec3 light_dir = normalize(light_pos - frag_pos);
  vec3 view_dir = normalize(view_pos - frag_pos);

  // Diffuse
  float diff = max(dot(norm, light_dir), 0.0);

  // Specular
  vec3  reflect_dir = reflect(-light_dir, norm);
  float spec = spec_strength * pow(max(dot(view_dir, reflect_dir), 0.0), shininess);

  // Shadow factor
  float shadow = 0.0;
  if (!bypass_shadow_map)
    shadow = calculate_shadow(frag_pos_light_space, light_dir, normal, true);

  spec *= (1.0 - shadow);

  // apply shadow
  if (true)
  {
    float diff_m = min(diff, 1.0 - shadow);
    diff_m = 1.0 - shadow_strength +
             shadow_strength * smoothstep(1.0 - shadow_strength, 1.0, diff_m);

    vec3 diffuse = color * diff_m;
    vec3 specular = spec_strength * spec * vec3(1.0);
    vec3 ambient = 0.2 * color;
    vec3 result = ambient + diffuse + specular;

    if (add_ambiant_occlusion)
    {
      // float ao = compute_AO(frag_uv,
      //                       texture_hmap,
      //                       ambiant_occlusion_radius,
      //                       ambiant_occlusion_strength);
      // ambient *= ao;

      float ao = compute_hbao(frag_uv, texture_hmap, 1.0, 8, 8);
      result *= ao;
    }

    frag_color = vec4(result, alpha);
  }

  // --- FOG

  if (add_fog)
  {
    float fog_density = 50.0;
    vec3  fog_color = vec3(1.0, 1.0, 1.0);
    float fog_height = 0.1;

    if (frag_pos.y > 0.0)
    {
      // fetch depth
      float depth_sample = texture(texture_depth, gl_FragCoord.xy / screen_size).r;

      // convert to view-space depth
      float view_depth = linearize_depth(depth_sample);
      float fog_factor = 1.0 - exp(-view_depth * fog_density);

      fog_factor *= exp(-frag_pos.y / fog_height);
      fog_factor = clamp(fog_factor, 0.0, 1.0);

      frag_color.xyz = mix(frag_color.xyz, fog_color, fog_factor);
    }
  }

  // --- ATMOSPHERIC SCATTERING

  if (add_atmospheric_scattering)
  {
    float fog_density = 0.2;
    int   num_steps = 32;
    vec3  fog_color = vec3(1.0, 1.0, 1.0);
    vec3  light_color = vec3(1.0, 1.0, 1.0);
    float hg_g = 0.7;
    float rayleigh_height = 0.8;
    float mie_height = 0.4;
    vec3  rayleigh_color = vec3(0.5, 0.7, 1.0); // bluish
    vec3  mie_color = vec3(1.0, 0.9, 0.8);      // whitish/yellowish

    float fog_strength = 0.5;
    float fog_scattering_ratio = 0.7;

    // Ray direction (from camera to fragment)
    vec3  ray_dir = normalize(frag_pos - camera_pos);
    float ray_length = length(frag_pos - camera_pos);

    // Ray-march from camera → fragment
    float step_size = ray_length / float(num_steps);
    vec3  step_vec = ray_dir * step_size;

    vec3 sample_pos = camera_pos;
    vec3 scattering = vec3(0.0);

    for (int i = 0; i < num_steps; i++)
    {
      sample_pos += step_vec;

      if (sample_pos.y < 0.0)
        continue;

      // simple exponential fog
      float dist = length(sample_pos - camera_pos);
      float density = exp(-fog_density * dist) * step_size;
      density *= (0.8 + 0.2 * hash(sample_pos * 0.1));

      // Shadow test
      vec4  light_space_pos = light_space_matrix * vec4(sample_pos, 1.0);
      float lit = calculate_shadow(light_space_pos, light_dir, normal, false);

      // In-scattering from directional light
      float cos_theta = dot(normalize(-light_dir), -ray_dir);
      float phase = phase_hg(cos_theta, hg_g);

      float rayleigh_factor = exp(-sample_pos.y / rayleigh_height);
      float mie_factor = exp(-sample_pos.y / mie_height);

      float pr = phase_rayleigh(cos_theta);
      float pm = phase_mie(cos_theta, hg_g);

      // soft cap for Mie scattering
      pm = pm / (1.0 + pm);

      // pr *= rayleigh_factor;
      // pm *= mie_factor;

      // scale by densities (tweak or make altitude-dependent)
      vec3 phase_color = rayleigh_color * pr + mie_color * pm;

      // float phase = max(dot(normalize(light_dir), -ray_dir), 0.0); // simple isotropic
      scattering += density * light_color * phase_color * (1.0 - lit);
    }

    // fog color with scattering
    vec3 fogged = mix(fog_color, scattering, fog_scattering_ratio);

    frag_color.xyz = mix(frag_color.xyz,
                         fogged,
                         clamp(fog_density * ray_length, 0.0, fog_strength));
  }

  if (apply_tonemap)
    frag_color = vec4(tonemap_ACES(frag_color.xyz), alpha);
}
)""
