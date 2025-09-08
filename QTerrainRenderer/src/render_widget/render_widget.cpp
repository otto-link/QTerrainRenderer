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

// TODO DBG
#include <cstdlib>

namespace qtr
{

RenderWidget::RenderWidget(const std::string &_title, QWidget *parent)
    : QOpenGLWidget(parent), title(_title)
{
  QTR_LOG->trace("RenderWidget::RenderWidget");

  this->setWindowTitle(this->title.c_str());
  this->setFocusPolicy(Qt::StrongFocus);
  this->setMouseTracking(true);

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
  ImGui_ImplOpenGL3_Shutdown();
  ImGui::DestroyContext();
  this->doneCurrent();
}

void RenderWidget::initializeGL()
{
  QTR_LOG->trace("RenderWidget::initializeGL");

  QOpenGLFunctions_3_3_Core::initializeOpenGLFunctions();

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

  // TODO Instance

  {
    float r = 0.001f;

    auto tree_mesh = std::make_shared<Mesh>();
    generate_tree(*tree_mesh, r, 0.1f * r, 5.f * r, r, 4);

    // glm::vec3(0.2f, rd(0.2f, 0.8f), 0.2f)

    auto rock_mesh = std::make_shared<Mesh>();
    generate_rock_mesh(*rock_mesh, r, 0.3f, 0, 2);

    auto sphere_mesh = std::make_shared<Mesh>();
    generate_sphere(*sphere_mesh, r);

    // 2. Fill instances
    std::vector<BaseInstance> instances;
    for (int i = 0; i < 50000; ++i)
    {
      auto rd = [](float a, float b) { return a + (b - a) * std::rand() / RAND_MAX; };

      float v = rd(0.3f, 0.5f);

      instances.push_back({glm::vec3(rd(-1, 1), 0.1f, rd(-1, 1)),
                           rd(0.5f, 2.0f),
                           rd(0.0f, glm::two_pi<float>()),
                           glm::vec3(v, v, v)});
    }

    instanced_mesh.create(rock_mesh, instances);
  }

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
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  imgui_set_blender_style();

  // OpenGL3 backend
  ImGui_ImplOpenGL3_Init("#version 330");
}

void RenderWidget::json_from(nlohmann::json const &json)
{
  QTR_LOG->trace("RenderWidget::json_from");

  // geometry
  if (json.contains("x") && json.contains("y") && json.contains("width") &&
      json.contains("height"))
  {
    int x = json["x"];
    int y = json["y"];
    int w = json["width"];
    int h = json["height"];

    this->setGeometry(x, y, w, h);
  }
  else
  {
    QTR_LOG->error("RenderWidget::json_from: could not parse the widget geometry data");
  }

  // data
  if (json.contains("title"))
    this->title = json["title"];
  else
    QTR_LOG->error("RenderWidget::json_from: could not parse the widget title data");
}

nlohmann::json RenderWidget::json_to() const
{
  QTR_LOG->trace("RenderWidget::json_to");

  nlohmann::json json;

  // geometry
  QRect geom = this->geometry();
  json["x"] = geom.x();
  json["y"] = geom.y();
  json["width"] = geom.width();
  json["height"] = geom.height();

  // data
  json["title"] = this->title;

  return json;
}

void RenderWidget::reset_camera_position()
{
  this->target = glm::vec3(0.0f, 0.0f, 0.0f);
  this->pan_offset = glm::vec2(0.0f, 0.0f);
  this->distance = 5.0f;
  this->alpha_x = 35.f / 180.f * 3.14f;
  this->alpha_y = -25.f / 180.f * 3.14f;

  this->light_phi = -45.f / 180.f * 3.14f;
  this->light_theta = 30.f / 180.f * 3.14f;
}

void RenderWidget::reset_heightmap_geometry()
{
  this->hmap.destroy();
  this->texture_hmap.destroy();
}

void RenderWidget::reset_water_geometry() { this->water_mesh.destroy(); }

void RenderWidget::reset_texture_albedo() { this->texture_albedo.destroy(); }

void RenderWidget::reset_texture_normal() { this->texture_normal.destroy(); }

void RenderWidget::reset_path() { this->path_mesh.destroy(); }

void RenderWidget::reset_points() { this->points_mesh.destroy(); }

void RenderWidget::resizeEvent(QResizeEvent *event)
{
  QOpenGLWidget::resizeEvent(event);

  // TODO update buffers
  this->repaint();
}

void RenderWidget::resizeGL(int w, int h)
{
  this->glViewport(0, 0, w, h);
  ImGui::GetIO().DisplaySize = ImVec2(float(w), float(h));
  this->repaint();
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
  shader.setUniformValue("scale_h", scale_h);
  shader.setUniformValue("hmap_h0", this->hmap_h0);
  shader.setUniformValue("hmap_h", this->hmap_h);
  shader.setUniformValue("normal_visualization", normal_visualization);
  shader.setUniformValue("normal_map_scaling", 0.f); // reset by default
  shader.setUniformValue("gamma_correction", gamma_correction);
  shader.setUniformValue("apply_tonemap", apply_tonemap);

  // Effects
  shader.setUniformValue("add_fog", add_fog);
  shader.setUniformValue("add_atmospheric_scattering", add_atmospheric_scattering);

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
}

void RenderWidget::set_path(const std::vector<float> &x,
                            const std::vector<float> &y,
                            const std::vector<float> &h)
{
  QTR_LOG->trace("RenderWidget::set_path");

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
}

void RenderWidget::set_points(const std::vector<float> &x,
                              const std::vector<float> &y,
                              const std::vector<float> &h)
{
  QTR_LOG->trace("RenderWidget::set_points");

  if (x.size() != y.size() || x.size() != h.size())
    throw std::invalid_argument("RenderWidget::set_points: vector sizes does not match");

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

  generate_downward_triangles(points_mesh, points, 0.05f, 0.01f);
}

void RenderWidget::set_texture_albedo(const std::vector<uint8_t> &data, int width)
{
  QTR_LOG->trace("RenderWidget::set_texture_albedo");

  this->texture_albedo.from_image_8bit_rgba(data, width);
}

void RenderWidget::set_texture_normal(const std::vector<uint8_t> &data, int width)
{
  QTR_LOG->trace("RenderWidget::set_texture_normal");

  this->texture_normal.from_image_8bit_rgb(data, width);
}

void RenderWidget::set_water_geometry(const std::vector<float> &data,
                                      int                       width,
                                      int                       height,
                                      float                     exclude_below)
{
  QTR_LOG->trace("RenderWidget::set_water_geometry");

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
