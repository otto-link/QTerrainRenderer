/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <stdexcept>

#include <QOpenGLFunctions>

#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include "qtr/config.hpp"
#include "qtr/gl_errors.hpp"
#include "qtr/imgui_widgets.hpp"
#include "qtr/logger.hpp"
#include "qtr/mesh.hpp"
#include "qtr/primitives.hpp"
#include "qtr/render_widget.hpp"
#include "qtr/utils.hpp"

namespace qtr
{

RenderWidget::RenderWidget(const std::string &_title, QWidget *parent)
    : QOpenGLWidget(parent), title(_title)
{
  QTR_LOG->trace("RenderWidget::RenderWidget");

  this->setWindowTitle(this->title.c_str());
  this->setFocusPolicy(Qt::StrongFocus);
  this->setMouseTracking(true);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // force 60 FPS update
  this->connect(&frame_timer,
                &QTimer::timeout,
                [this]()
                {
                  if (this->need_update)
                  {
                    this->update();
                    this->need_update = false;
                  }
                });
  this->frame_timer.start(16);

  // init.
  this->timer.start(); // global timer
  this->reset_camera_position();
}

RenderWidget::~RenderWidget()
{
  this->makeCurrent();
  ImGui::SetCurrentContext(this->imgui_context);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui::DestroyContext(this->imgui_context);
  this->doneCurrent();

  this->imgui_context = nullptr;
}

void RenderWidget::clear()
{
  QTR_LOG->trace("RenderWidget::clear");

  this->makeCurrent();

  this->reset_heightmap_geometry();
  this->reset_water_geometry();
  this->reset_points();
  this->reset_path();
  this->reset_rocks();
  this->reset_trees();
  this->reset_texture_albedo();
  this->reset_texture_normal();

  this->need_update = true;

  this->doneCurrent();
}

ImGuiIO &RenderWidget::get_imgui_io()
{
  ImGui::SetCurrentContext(this->imgui_context);
  return ImGui::GetIO();
}

float RenderWidget::get_water_elevation() const { return this->water_elevation; }

void RenderWidget::initializeGL()
{
  QTR_LOG->trace("RenderWidget::initializeGL");

  this->makeCurrent();
  this->initializeOpenGLFunctions();

  // shaders (NB, context needs to be set beforehand...)
  QTR_LOG->trace("RenderWidget::initializeGL: setting up shaders...");

  this->sp_shader_manager = std::make_unique<ShaderManager>();

  this->sp_shader_manager->add_shader_from_code("diffuse_basic",
                                                diffuse_basic_vertex,
                                                diffuse_basic_frag);

  this->sp_shader_manager->add_shader_from_code("diffuse_phong",
                                                diffuse_basic_vertex,
                                                diffuse_phong_frag);

  this->sp_shader_manager->add_shader_from_code("diffuse_blinn_phong",
                                                diffuse_basic_vertex,
                                                diffuse_blinn_phong_frag);

  this->sp_shader_manager->add_shader_from_code("depth_map",
                                                depth_map_vertex,
                                                depth_map_frag);

  this->sp_shader_manager->add_shader_from_code("shadow_map_depth_pass",
                                                shadow_map_depth_pass_vertex,
                                                shadow_map_depth_pass_frag);

  this->sp_shader_manager->add_shader_from_code("shadow_map_lit_pass",
                                                shadow_map_lit_pass_vertex,
                                                shadow_map_lit_pass_frag);

  generate_plane(this->plane, 0.f, 0.f, 0.f, 4.f, 4.f);
  this->update_water_plane();

  // depth buffer
  int depth_map_res = 256;
  this->texture_depth.generate_depth_texture(depth_map_res, depth_map_res, false);

  // Create framebuffer for depth map
  {
    glGenFramebuffers(1, &this->fbo_depth);
    glBindFramebuffer(GL_FRAMEBUFFER, this->fbo_depth);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D,
                           this->texture_depth.get_id(),
                           0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    check_gl_error("Texture::initializeGL: attach FBO");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  // shadow map texture and buffer
  int shadow_map_res = 2048;
  this->texture_shadow_map.generate_depth_texture(shadow_map_res, shadow_map_res, true);

  // Create framebuffer for shadow depth
  {
    glGenFramebuffers(1, &this->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, this->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D,
                           this->texture_shadow_map.get_id(),
                           0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    check_gl_error("Texture::initializeGL: attach FBO");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  QTR_LOG->trace("RenderWidget::initializeGL: setup ImGui context");

  // ImGui context
  IMGUI_CHECKVERSION();
  this->imgui_context = ImGui::CreateContext();
  ImGui::SetCurrentContext(this->imgui_context);
  ImGui::StyleColorsDark();
  imgui_set_blender_style();

  // OpenGL3 backend
  ImGui_ImplOpenGL3_Init("#version 330");

  this->doneCurrent();
}

void RenderWidget::reset_camera_position()
{
  // TODO use json
  this->target = glm::vec3(0.0f, 0.0f, 0.0f);
  this->pan_offset = glm::vec2(0.0f, 0.0f);
  this->distance = 5.0f;
  this->alpha_x = 35.f / 180.f * 3.14f;
  this->alpha_y = -25.f / 180.f * 3.14f;

  this->light_phi = -45.f / 180.f * 3.14f;
  this->light_theta = 30.f / 180.f * 3.14f;

  this->need_update = true;
}

void RenderWidget::reset_heightmap_geometry()
{
  this->makeCurrent();
  this->hmap.destroy();
  this->texture_hmap.destroy();
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::reset_water_geometry()
{
  this->water_mesh.destroy();
  this->need_update = true;
}

void RenderWidget::reset_texture_albedo()
{
  this->texture_albedo.destroy();
  this->need_update = true;
}

void RenderWidget::reset_texture_normal()
{
  this->texture_normal.destroy();
  this->need_update = true;
}

void RenderWidget::reset_path()
{
  this->path_mesh.destroy();
  this->need_update = true;
}

void RenderWidget::reset_points()
{
  this->points_instanced_mesh.destroy();
  this->need_update = true;
}

void RenderWidget::reset_rocks()
{
  this->rocks_instanced_mesh.destroy();
  this->need_update = true;
}

void RenderWidget::reset_trees()
{
  this->trees_instanced_mesh.destroy();
  this->need_update = true;
}

void RenderWidget::resizeEvent(QResizeEvent *event)
{
  QOpenGLWidget::resizeEvent(event);
  this->need_update = true;
}

void RenderWidget::resizeGL(int w, int h)
{
  this->glViewport(0, 0, w, h);
  this->get_imgui_io().DisplaySize = ImVec2(float(w), float(h));
  this->need_update = true;
}

void RenderWidget::set_common_uniforms(QOpenGLShaderProgram &shader,
                                       const glm::mat4      &model,
                                       const glm::mat4      &projection,
                                       const glm::mat4      &view,
                                       const glm::mat4      &light_space)
{
  // Textures
  this->texture_albedo.bind_and_set(shader, "texture_albedo", 0);
  this->texture_hmap.bind_and_set(shader, "texture_hmap", 1);
  this->texture_normal.bind_and_set(shader, "texture_normal", 2);
  this->texture_shadow_map.bind_and_set(shader, "texture_shadow_map", 3);
  this->texture_depth.bind_and_set(shader, "texture_depth", 4);

  // Time & matrices
  shader.setUniformValue("time", time);
  shader.setUniformValue("model", toQMat(model));
  shader.setUniformValue("view", toQMat(view));
  shader.setUniformValue("projection", toQMat(projection));
  shader.setUniformValue("light_space_matrix", toQMat(light_space));

  // Camera & light
  shader.setUniformValue("camera_pos", toQVec(camera.position));
  shader.setUniformValue("view_pos", toQVec(camera.position));
  shader.setUniformValue("light_pos", toQVec(light.position));

  // Screen & depth
  shader.setUniformValue("screen_size", toQVec(glm::vec2(width(), height())));
  shader.setUniformValue("near_plane", camera.near_plane);
  shader.setUniformValue("far_plane", camera.far_plane);

  // Shadow & AO
  shader.setUniformValue("bypass_shadow_map", bypass_shadow_map);
  shader.setUniformValue("shadow_strength", shadow_strength);
  shader.setUniformValue("add_ambiant_occlusion", add_ambiant_occlusion);
  shader.setUniformValue("ambiant_occlusion_strength", ambiant_occlusion_strength);
  shader.setUniformValue("ambiant_occlusion_radius", ambiant_occlusion_radius);

  // Rendering settings
  shader.setUniformValue("has_instances", false);
  shader.setUniformValue("scale_h", scale_h);
  shader.setUniformValue("hmap_h0", this->hmap_h0);
  shader.setUniformValue("hmap_h", this->hmap_h);
  shader.setUniformValue("normal_visualization", normal_visualization);
  shader.setUniformValue("normal_map_scaling", 0.f); // reset by default
  shader.setUniformValue("gamma_correction", gamma_correction);
  shader.setUniformValue("apply_tonemap", apply_tonemap);

  // Effects
  shader.setUniformValue("add_fog", add_fog);
  shader.setUniformValue("fog_color", toQVec(fog_color));
  shader.setUniformValue("fog_density", fog_density);
  shader.setUniformValue("fog_height", fog_height);
  shader.setUniformValue("add_atmospheric_scattering", add_atmospheric_scattering);
  shader.setUniformValue("scattering_density", scattering_density);
  shader.setUniformValue("rayleigh_color", toQVec(rayleigh_color));
  shader.setUniformValue("mie_color", toQVec(mie_color));
  shader.setUniformValue("fog_strength", fog_strength);
  shader.setUniformValue("fog_scattering_ratio", fog_scattering_ratio);

  // Reset per-object flags
  shader.setUniformValue("use_texture_albedo", false);
  shader.setUniformValue("use_water_colors", false);
  shader.setUniformValue("normal_map_scaling", 0.f);
  shader.setUniformValue("shininess", 32.f);
  shader.setUniformValue("spec_strength", 0.f);
}

void RenderWidget::set_heightmap_geometry(const std::vector<float> &data,
                                          int                       width,
                                          int                       height,
                                          bool                      add_skirt)
{
  QTR_LOG->trace("RenderWidget::set_heightmap_geometry");

  this->makeCurrent();

  generate_heightmap(this->hmap,
                     data,
                     width,
                     height,
                     0.f,
                     this->hmap_h0,
                     0.f,
                     this->hmap_w,
                     this->hmap_h,
                     this->hmap_w,
                     add_skirt,
                     0.f);

  QTR_LOG->trace("RenderWidget::set_heightmap_geometry: w x h = {} x {}", width, height);

  // also generate the heightmap texture
  this->texture_hmap.from_float_vector(data, width);
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::set_path(const std::vector<float> &x,
                            const std::vector<float> &y,
                            const std::vector<float> &h)
{
  QTR_LOG->trace("RenderWidget::set_path");

  this->makeCurrent();

  if (x.size() != y.size() || x.size() != h.size())
    throw std::invalid_argument("RenderWidget::set_path: vector sizes does not match");

  std::vector<glm::vec3> points;
  for (size_t k = 0; k < x.size(); ++k)
  {
    // rescale to render size
    float xs = 0.5f * this->hmap_w * (2.f * x[k] - 1.f);
    float hs = this->hmap_h0 + this->hmap_h * h[k];
    float ys = 0.5f * this->hmap_w * (2.f * y[k] - 1.f);

    points.push_back(glm::vec3(xs, hs, ys));
  }

  // TODO scale with point value

  generate_path(path_mesh, points, 0.01f);
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::set_points(const std::vector<float> &x,
                              const std::vector<float> &y,
                              const std::vector<float> &h)
{
  QTR_LOG->trace("RenderWidget::set_points");

  this->makeCurrent();

  if (x.size() != y.size() || x.size() != h.size())
    throw std::invalid_argument("RenderWidget::set_points: vector sizes does not match");

  std::vector<BaseInstance> instances;

  float     scale = 0.01f;
  float     rotation = 0.f;
  glm::vec3 color = glm::vec3(0.f, 1.f, 0.);

  for (size_t k = 0; k < x.size(); ++k)
  {
    float xs = 0.5f * this->hmap_w * (2.f * x[k] - 1.f);
    float hs = this->hmap_h0 + this->hmap_h * h[k];
    float ys = 0.5f * this->hmap_w * (2.f * y[k] - 1.f);

    instances.push_back({glm::vec3(xs, hs, ys), scale, rotation, color});
  }

  // unit sphere
  auto sphere_mesh = std::make_shared<Mesh>();
  generate_sphere(*sphere_mesh, 1.f);

  this->points_instanced_mesh.create(sphere_mesh, instances);
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::set_rocks(const std::vector<float> &x,
                             const std::vector<float> &y,
                             const std::vector<float> &h,
                             const std::vector<float> &radius)
{
  QTR_LOG->trace("RenderWidget::set_rocks");

  this->makeCurrent();

  if (x.size() != y.size() || x.size() != h.size() || x.size() != radius.size())
    throw std::invalid_argument("RenderWidget::set_rocks: vector sizes does not match");

  std::vector<BaseInstance> instances;

  glm::vec3 color = glm::vec3(0.f, 1.f, 0.);

  for (size_t k = 0; k < x.size(); ++k)
  {
    float xs = 0.5f * this->hmap_w * (2.f * x[k] - 1.f);
    float hs = this->hmap_h0 + this->hmap_h * h[k];
    float ys = 0.5f * this->hmap_w * (2.f * y[k] - 1.f);
    float rs = 2.f * radius[k];
    float rotation = std::rand() / RAND_MAX * glm::two_pi<float>();

    instances.push_back({glm::vec3(xs, hs, ys), rs, rotation, color});
  }

  // unit sphere
  auto mesh = std::make_shared<Mesh>();
  generate_rock(*mesh, 1.f, 0.3f, 0);

  this->rocks_instanced_mesh.create(mesh, instances);
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::set_texture_albedo(const std::vector<uint8_t> &data, int width)
{
  QTR_LOG->trace("RenderWidget::set_texture_albedo");

  this->makeCurrent();

  this->texture_albedo.from_image_8bit_rgba(data, width);
  this->need_update = true;
}

void RenderWidget::set_texture_normal(const std::vector<uint8_t> &data, int width)
{
  QTR_LOG->trace("RenderWidget::set_texture_normal");

  this->makeCurrent();

  this->texture_normal.from_image_8bit_rgba(data, width);
  this->need_update = true;
}

void RenderWidget::set_trees(const std::vector<float> &x,
                             const std::vector<float> &y,
                             const std::vector<float> &h,
                             const std::vector<float> &radius)
{
  QTR_LOG->trace("RenderWidget::set_trees");

  this->makeCurrent();

  if (x.size() != y.size() || x.size() != h.size() || x.size() != radius.size())
    throw std::invalid_argument("RenderWidget::set_trees: vector sizes does not match");

  std::vector<BaseInstance> instances;

  glm::vec3 color = glm::vec3(0.f, 1.f, 0.);

  for (size_t k = 0; k < x.size(); ++k)
  {
    float xs = 0.5f * this->hmap_w * (2.f * x[k] - 1.f);
    float hs = this->hmap_h0 + this->hmap_h * h[k];
    float ys = 0.5f * this->hmap_w * (2.f * y[k] - 1.f);
    float rs = 2.f * radius[k];
    float rotation = std::rand() / RAND_MAX * glm::two_pi<float>();

    instances.push_back({glm::vec3(xs, hs, ys), rs, rotation, color});
  }

  // unit sphere
  auto  mesh = std::make_shared<Mesh>();
  float r = 1.f;
  generate_tree(*mesh, r, 0.1f * r, 5.f * r, r, 5);

  this->trees_instanced_mesh.create(mesh, instances);
  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::set_water_geometry(const std::vector<float> &data,
                                      int                       width,
                                      int                       height,
                                      float                     exclude_below)
{
  QTR_LOG->trace("RenderWidget::set_water_geometry");

  this->makeCurrent();

  bool  add_skirt = false;
  float add_level = 0.f;

  generate_heightmap(this->water_mesh,
                     data,
                     width,
                     height,
                     0.f,
                     this->hmap_h0,
                     0.f,
                     this->hmap_w,
                     this->hmap_h,
                     this->hmap_w,
                     add_skirt,
                     add_level,
                     exclude_below);

  this->need_update = true;
  this->doneCurrent();
}

void RenderWidget::setup_gl_state()
{
  glViewport(0, 0, this->width(), this->height());
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  glPolygonMode(GL_FRONT_AND_BACK, this->wireframe_mode ? GL_LINE : GL_FILL);
}

QSize RenderWidget::sizeHint() const { return QSize(QTR_CONFIG->widget.size_hint); }

void RenderWidget::unbind_textures()
{
  this->texture_albedo.unbind();
  this->texture_hmap.unbind();
  this->texture_normal.unbind();
  this->texture_shadow_map.unbind();
  this->texture_depth.unbind();
}

void RenderWidget::update_camera()
{
  this->camera.set_position_angles(this->distance, this->alpha_x, this->alpha_y);

  glm::vec3 pan(this->pan_offset.x * cos(this->alpha_y),
                this->pan_offset.y,
                -this->pan_offset.x * sin(this->alpha_y));

  this->camera.position += pan;
  this->camera.target = this->target + pan;
}

void RenderWidget::update_light()
{
  this->light.set_position_spherical(this->light_distance,
                                     this->light_theta,
                                     this->light_phi);

  // actually works with a fixed sun: compensate for the elevation
  // scaling
  this->light.position.y /= this->scale_h;

  if (this->auto_rotate_light)
  {
    this->light_phi += 0.5f * this->dt;
    this->need_update = true;
  }
}

void RenderWidget::update_time()
{
  this->dt = static_cast<float>(this->timer.restart()) / 1000.0f;
  this->time += this->dt;
}

void RenderWidget::update_water_plane()
{
  generate_plane(this->water_plane,
                 0.f,
                 this->hmap_h0 + this->hmap_h * this->water_elevation,
                 0.f,
                 2.f,
                 2.f);
}

} // namespace qtr
