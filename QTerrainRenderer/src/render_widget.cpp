/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
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

  this->sp_shader_manager->add_shader_from_code("shadow_map_depth_pass",
                                                shadow_map_depth_pass_vertex,
                                                shadow_map_depth_pass_frag);

  this->sp_shader_manager->add_shader_from_code("shadow_map_lit_pass",
                                                shadow_map_lit_pass_vertex,
                                                shadow_map_lit_pass_frag);

  generate_plane(this->plane, 0.f, 0.f, 0.f, 4.f, 4.f);

  generate_plane(this->water_plane, 0.f, 0.2f + this->water_elevation, 0.f, 2.f, 2.f);

  std::vector<glm::vec3> points = {{0.0f, 0.6f, 0.0f},
                                   {0.2f, 0.7f, 0.2f},
                                   {-0.3f, 0.8f, 0.1f}};

  generate_downward_triangles(points_mesh, points, 0.1f, 0.01f);

  {
    int                width, height;
    std::vector<float> data = load_png_as_grayscale("hmap.png", width, height);
    generate_heightmap(this->hmap,
                       data,
                       width,
                       height,
                       0.f,
                       0.f,
                       0.2f,
                       2.f,
                       2.f,
                       1.f,
                       true,
                       0.f);
  }

  {
    int width, height;
    // std::vector<uint8_t> data = load_png_as_8bit_rgba("texture_white.png", width,
    // height);
    std::vector<uint8_t> data = load_png_as_8bit_rgba("texture.png", width, height);
    this->texture_albedo.from_image_8bit_rgba(data, width);
  }

  {
    int                   width, height;
    std::vector<uint16_t> data = load_png_as_16bit_grayscale("hmap.png", width, height);
    this->texture_hmap.from_image_16bit_grayscale(data, width);
  }

  // shadow map texture and buffer
  int shadow_map_res = 2048;
  this->texture_shadow_map.generate_depth_texture(shadow_map_res, shadow_map_res);

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

void RenderWidget::paintGL()
{
  // calculate dt (in seconds)
  float dt = static_cast<float>(timer.restart()) / 1000.0f;
  this->time += dt;

  // --- FIRST - OpenGL scene rendering

  // MODEL
  glm::mat4 model = glm::mat4(1.0f);
  model = glm::scale(model, glm::vec3(1.f, this->scale_h, 1.f));

  // LIGHT
  this->light.set_position_spherical(this->light_distance,
                                     this->light_theta,
                                     this->light_phi);

  // shadow depth pass, camera at the light position
  this->camera_shadow_pass.position = this->light.position;
  this->camera_shadow_pass.near_plane = 0.f;
  this->camera_shadow_pass.far_plane = 100.f;

  float     ortho_size = 1.5f; // TODO hardcoded
  glm::mat4 light_projection = this->camera_shadow_pass.get_projection_matrix_ortho(
      ortho_size);
  glm::mat4 light_view = this->camera_shadow_pass.get_view_matrix();
  glm::mat4 light_space_matrix = light_projection * light_view;

  {
    QOpenGLShaderProgram *p_shader = this->sp_shader_manager->get("shadow_map_depth_pass")
                                         ->get();

    if (p_shader)
    {
      // backup FBO state to avoid messing up with others FBO (ImGUI
      // for instance...)
      GLint previous_fbo;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);

      glViewport(0,
                 0,
                 this->texture_shadow_map.get_width(),
                 this->texture_shadow_map.get_height());
      glBindFramebuffer(GL_FRAMEBUFFER, this->fbo);

      glClear(GL_DEPTH_BUFFER_BIT);
      glEnable(GL_DEPTH_TEST);
      glCullFace(GL_FRONT);

      p_shader->bind();
      p_shader->setUniformValue("light_space_matrix", toQMat(light_space_matrix));
      p_shader->setUniformValue("model", toQMat(model));

      plane.draw();
      hmap.draw();
      // water_plane.draw();
      points_mesh.draw();

      p_shader->release();

      glCullFace(GL_BACK);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      check_gl_error("Texture::paintGL: render shadow pass");

      // set previous FBO back
      glBindFramebuffer(GL_FRAMEBUFFER, previous_fbo);
    }
  }

  {
    // transformation matrices

    // VIEW

    this->camera.set_position_angles(this->distance, this->alpha_x, this->alpha_y);

    glm::vec3 pan(this->pan_offset.x * cos(this->alpha_y),
                  this->pan_offset.y,
                  -this->pan_offset.x * sin(this->alpha_y));

    this->camera.position += pan;
    this->camera.target = this->target + pan;

    // PROJECTION
    float aspect_ratio = static_cast<float>(this->width()) /
                         static_cast<float>(this->height());

    glm::mat4 projection = this->camera.get_projection_matrix_perspective(aspect_ratio);

    // DRAW CALL
    glViewport(0, 0, this->width(), this->height());
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    if (this->wireframe_mode)
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    QOpenGLShaderProgram *p_shader = this->sp_shader_manager->get("shadow_map_lit_pass")
                                         ->get();

    if (p_shader)
    {
      p_shader->bind();
      p_shader->setUniformValue("screen_size",
                                toQVec(glm::vec2(this->width(), this->height())));
      p_shader->setUniformValue("time", this->time);

      p_shader->setUniformValue("model", toQMat(model));
      p_shader->setUniformValue("view", toQMat(camera.get_view_matrix()));
      p_shader->setUniformValue("projection", toQMat(projection));

      p_shader->setUniformValue("camera_pos", toQVec(this->camera.position));
      p_shader->setUniformValue("light_space_matrix", toQMat(light_space_matrix));
      p_shader->setUniformValue("light_pos", toQVec(this->light.position));
      p_shader->setUniformValue("view_pos", toQVec(this->camera.position));
      p_shader->setUniformValue("shininess", 32.f);
      p_shader->setUniformValue("spec_strength", 0.f);
      p_shader->setUniformValue("bypass_shadow_map", this->bypass_shadow_map);
      p_shader->setUniformValue("shadow_strength", this->shadow_strength);
      p_shader->setUniformValue("add_ambiant_occlusion", this->add_ambiant_occlusion);
      p_shader->setUniformValue("ambiant_occlusion_strength",
                                this->ambiant_occlusion_strength);
      p_shader->setUniformValue("ambiant_occlusion_radius",
                                this->ambiant_occlusion_radius);
      p_shader->setUniformValue("use_texture_albedo", false);
      p_shader->setUniformValue("use_water_colors", false);
      p_shader->setUniformValue("gamma_correction", this->gamma_correction);
      p_shader->setUniformValue("apply_tonemap", this->apply_tonemap);

      p_shader->setUniformValue("texture_albedo", 0);
      p_shader->setUniformValue("texture_hmap", 1);
      p_shader->setUniformValue("texture_shadow_map", 2);

      this->texture_albedo.bind(0);
      this->texture_hmap.bind(1);
      this->texture_shadow_map.bind(2);

      p_shader->setUniformValue("base_color", QVector3D(0.5f, 0.5f, 0.5f));
      p_shader->setUniformValue("add_ambiant_occlusion", false);
      plane.draw();

      p_shader->setUniformValue("base_color", QVector3D(0.f, 1.f, 0.f));
      p_shader->setUniformValue("add_ambiant_occlusion", false);
      points_mesh.draw();

      p_shader->setUniformValue("base_color", QVector3D(0.7f, 0.7f, 0.7f));
      p_shader->setUniformValue("use_texture_albedo",
                                true && !this->bypass_texture_albedo);
      p_shader->setUniformValue("add_ambiant_occlusion", this->add_ambiant_occlusion);
      hmap.draw();

      if (this->add_water)
      {
        p_shader->setUniformValue("spec_strength", 1.f);

        p_shader->setUniformValue("add_ambiant_occlusion", false);
        p_shader->setUniformValue("use_texture_albedo", false);

        p_shader->setUniformValue("use_water_colors", true);

        p_shader->setUniformValue("color_shallow_water",
                                  toQVec(this->color_shallow_water));
        p_shader->setUniformValue("color_deep_water", toQVec(this->color_deep_water));

        p_shader->setUniformValue("water_color_depth", this->water_color_depth);

        p_shader->setUniformValue("add_water_foam", this->add_water_foam);
        p_shader->setUniformValue("foam_color", toQVec(this->foam_color));
        p_shader->setUniformValue("foam_depth", this->foam_depth);

        p_shader->setUniformValue("add_water_waves", this->add_water_waves);
        p_shader->setUniformValue("angle_spread_ratio", this->angle_spread_ratio);
        p_shader->setUniformValue("waves_alpha", this->waves_alpha);
        p_shader->setUniformValue("waves_kw", this->waves_kw);
        p_shader->setUniformValue("waves_amplitude", this->waves_amplitude);
        p_shader->setUniformValue("waves_normal_amplitude", this->waves_normal_amplitude);

        if (this->animate_waves)
          p_shader->setUniformValue("waves_speed", this->waves_speed);
        else
          p_shader->setUniformValue("waves_speed", 0.f);

        water_plane.draw();
      }

      this->texture_shadow_map.unbind();
      this->texture_albedo.unbind();

      p_shader->release();
    }
  }

  // --- LAST - ImGui overlay rendering

  ImGui_ImplOpenGL3_NewFrame();
  ImGui::NewFrame();

  // FPS
  if (true)
  {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);

    const std::string str = this->title + " - FPS: %.1f";

    ImGui::Begin("TopLeftText", nullptr, flags);
    ImGui::Text(str.c_str(), ImGui::GetIO().Framerate);
    ImGui::End();
  }

  ImGui::Begin("Terrain Renderer");
  ImGui::Text("View");

  bool ret = false;

  ret |= ImGui::Checkbox("Wireframe mode", &this->wireframe_mode);
  ret |= ImGui::SliderFloat("scale_h", &this->scale_h, 0.f, 1.f);
  ret |= ImGui::SliderAngle("FOV", &this->fov, 10.f, 180.f);

  ImGui::Text("Light");
  ret |= ImGui::SliderAngle("Azimuth", &this->light_phi, -180.f, 180.f);
  ret |= ImGui::SliderAngle("Zenith", &this->light_theta, 0.f, 90.f);
  ImGui::Checkbox("Auto rotate light", &this->auto_rotate_light);

  ImGui::Text("Shadow map");
  ret |= ImGui::Checkbox("Bypass shadow map", &this->bypass_shadow_map);
  ret |= ImGui::SliderFloat("Shadow strength", &this->shadow_strength, 0.f, 1.f);

  ImGui::Text("Ambiant occlusion");
  ret |= ImGui::Checkbox("Add ambiant occlusion", &this->add_ambiant_occlusion);
  ret |= ImGui::SliderFloat("Ambiant occlusion strength",
                            &this->ambiant_occlusion_strength,
                            0.f,
                            1000.f);
  ret |= ImGui::SliderInt("Ambiant occlusion radius",
                          &this->ambiant_occlusion_radius,
                          0,
                          32);

  if (this->auto_rotate_light)
  {
    this->light_phi += 0.5f * dt;
    this->need_update = true;
  }

  if (ImGui::Button("Reset view"))
  {
    this->reset_camera_position();
    this->need_update = true;
  }

  ImGui::Text("Albedo");
  ret |= ImGui::SliderFloat("Gamma correction", &this->gamma_correction, 0.01f, 4.f);
  ret |= ImGui::Checkbox("Bypass albedo texture", &this->bypass_texture_albedo);
  ret |= ImGui::Checkbox("Apply tonemap", &this->apply_tonemap);

  ImGui::SeparatorText("Water");

  ImGui::Text("Water");
  {
    ret |= ImGui::Checkbox("Add water", &this->add_water);
    if (ImGui::SliderFloat("Water elevation", &this->water_elevation, 0.f, 1.f))
    {
      ret = true;
      generate_plane(this->water_plane,
                     0.f,
                     0.2f + this->water_elevation,
                     0.f,
                     2.f,
                     2.f); // TODO hardcoded
    }

    ret |= ImGui::SliderFloat("Water color depth", &this->water_color_depth, 0.f, 0.2f);

    ret |= imgui_show_water_preset_selector(this->color_shallow_water,
                                            this->color_deep_water);

    ret |= ImGui::Checkbox("Add foam", &this->add_water_foam);
    ret |= ImGui::SliderFloat("Foam depth", &this->foam_depth, 0.f, 0.1f);

    ret |= ImGui::Checkbox("Add waves", &this->add_water_waves);
    ret |= ImGui::SliderFloat("Waves wavenumber", &this->waves_kw, 0.f, 2048.f);
    ret |= ImGui::SliderFloat("Waves amplitude", &this->waves_amplitude, 0.f, 0.1f);
    ret |= ImGui::SliderFloat("Waves normal amplitude",
                              &this->waves_normal_amplitude,
                              0.f,
                              0.1f);
    ret |= ImGui::SliderAngle("Waves angle", &this->waves_alpha, -180.f, 180.f);
    ret |= ImGui::SliderFloat("angle_spread_ratio", &this->angle_spread_ratio, 0.f, 0.1f);

    ret |= ImGui::Checkbox("Animate waves", &this->animate_waves);
    ret |= ImGui::SliderFloat("Waves speed", &this->waves_speed, 0.f, 1.f);
    // force update if animation requested
    if (this->animate_waves)
      this->need_update = true;

    // const char *items[] = {"AAAA",
    //                        "BBBB",
    //                        "CCCC",
    //                        "DDDD",
    //                        "EEEE",
    //                        "FFFF",
    //                        "GGGG",
    //                        "HHHH",
    //                        "IIIIIII",
    //                        "JJJJ",
    //                        "KKKKKKK"};
    // static int  item_current = 0;
    // if (ImGui::Combo("combo", &item_current, items, IM_ARRAYSIZE(items)))
    // {
    //   ret = true;
    //   QTR_LOG->trace("{}", item_current);
    // }
  }

  this->need_update |= ret;

  // ImGUI IO
  if (!ret) // to prevent capture of widget mouse motions (partially...)
  {
    ImGuiIO &io = ImGui::GetIO();

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
      this->alpha_y -= io.MouseDelta.x * 0.005f; // horizontally
      this->alpha_x += io.MouseDelta.y * 0.005f; // vertically

      this->alpha_x = glm::clamp(this->alpha_x,
                                 -0.99f * glm::half_pi<float>(),
                                 0.99f * glm::half_pi<float>());
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
      this->pan_offset.x -= io.MouseDelta.x * 0.001f * this->distance;
      this->pan_offset.y += io.MouseDelta.y * 0.001f * this->distance;
    }

    if (io.MouseWheel != 0.0f)
    {
      this->distance *= (1.0f - io.MouseWheel * 0.1f);
      this->distance = glm::clamp(this->distance, 0.f, 50.0f);
    }
  }

  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  // --- check for errors...

  {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
      QTR_LOG->error("RenderWidget::paintGL: OpenGL error: {}", err);
  }
}

void RenderWidget::reset_camera_position()
{
  this->target = glm::vec3(0.0f, 0.0f, 0.0f);
  this->pan_offset = glm::vec2(0.0f, 0.0f);
  this->distance = 5.0f;
  this->alpha_x = 35.f / 180.f * 3.14f;
  this->alpha_y = -25.f / 180.f * 3.14f;
  this->fov = 45.f / 180.f * 3.14f;

  this->light_phi = -45.f / 180.f * 3.14f;
  this->light_theta = 30.f / 180.f * 3.14f;

  // this->light_phi = -3.f / 180.f * 3.14f;
  // this->light_theta = 10.f / 180.f * 3.14f;
}

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

QSize RenderWidget::sizeHint() const { return QSize(QTR_CONFIG->widget.size_hint); }

} // namespace qtr
