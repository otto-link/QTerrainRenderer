/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <QOpenGLFunctions>

#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include "qtr/config.hpp"
#include "qtr/gl_errors.hpp"
#include "qtr/logger.hpp"
#include "qtr/mesh.hpp"
#include "qtr/primitives.hpp"
#include "qtr/render_widget.hpp"

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

  generate_cube(this->cube, 0.f, 0.5, 0.0f, 1.f, 1.f, 1.f);
  generate_plane(this->plane, 0.f, 0.f, 0.f, 2.f, 2.f);

  this->shadow_depth_texture.generate_depth_texture(1024, 1024);

  // Create framebuffer for shadow depth
  {
    glGenFramebuffers(1, &this->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, this->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D,
                           this->shadow_depth_texture.get_id(),
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

  // --- FIRST - OpenGL scene rendering

  // MODEL
  glm::mat4 model = glm::mat4(1.0f);
  model = glm::scale(model, glm::vec3(1.f, this->scale_h, 1.f));

  // shadow depth pass
  glm::vec3 light_pos;
  float     light_distance = 10.f;

  light_pos.x = light_distance * cos(this->light_theta) * sin(this->light_phi);
  light_pos.y = light_distance * sin(this->light_theta);
  light_pos.z = light_distance * cos(this->light_theta) * cos(this->light_phi);

  glm::mat4 light_projection, light_view, light_space_matrix;
  float     near_plane = 1.0f;
  float     far_plane = 20.0f;
  float     ortho_size = 4.0f;

  light_projection = glm::ortho(-ortho_size,
                                ortho_size,
                                -ortho_size,
                                ortho_size,
                                near_plane,
                                far_plane);
  light_view = glm::lookAt(light_pos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
  light_space_matrix = light_projection * light_view;

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
                 this->shadow_depth_texture.get_width(),
                 this->shadow_depth_texture.get_height());
      glBindFramebuffer(GL_FRAMEBUFFER, this->fbo);

      glClear(GL_DEPTH_BUFFER_BIT);
      glEnable(GL_DEPTH_TEST);
      glCullFace(GL_FRONT);

      p_shader->bind();
      p_shader->setUniformValue("light_space_matrix", toQMat(light_space_matrix));
      p_shader->setUniformValue("model", toQMat(model));

      plane.draw();
      cube.draw();

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

    // MODEL
    // glm::mat4 model = glm::mat4(1.0f);
    // model = glm::scale(model, glm::vec3(1.f, this->scale_h, 1.f));

    // VIEW
    glm::vec3 camera_pos;

    camera_pos = this->target;
    camera_pos.x += this->distance * cos(this->alpha_x) * sin(this->alpha_y);
    camera_pos.y += this->distance * sin(this->alpha_x);
    camera_pos.z += this->distance * cos(this->alpha_x) * cos(this->alpha_y);

    glm::vec3 pan(this->pan_offset.x * cos(this->alpha_y),
                  this->pan_offset.y,
                  -this->pan_offset.x * sin(this->alpha_y));
    camera_pos += pan;

    glm::vec3 look_target = this->target + pan;
    glm::mat4 view = glm::lookAt(camera_pos, look_target, glm::vec3(0.f, 1.f, 0.f));

    // PROJECTION
    float aspect_ratio = static_cast<float>(this->width()) /
                         static_cast<float>(this->height());

    glm::mat4 projection = glm::perspective(this->fov,
                                            aspect_ratio,
                                            this->near_plane,
                                            this->far_plane);

    // LIGHT
    glm::vec3 light_dir(sin(this->light_theta) * sin(this->light_phi),
                        cos(this->light_theta),
                        sin(this->light_theta) * cos(this->light_phi));

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

    // QOpenGLShaderProgram *p_shader =
    // this->sp_shader_manager->get("diffuse_basic")->get();

    // if (p_shader)
    // {
    //   p_shader->bind();
    //   p_shader->setUniformValue("model", toQMat(model));
    //   p_shader->setUniformValue("view", toQMat(view));
    //   p_shader->setUniformValue("projection", toQMat(projection));

    //   p_shader->setUniformValue("color", QVector3D(0.8f, 0.3f, 0.2f)); // object color
    //   p_shader->setUniformValue("light_dir", toQVec(light_dir));

    //   // p_shader->setUniformValue("view_pos", toQVec(camera_pos));
    //   // p_shader->setUniformValue("shininess", 32.0f);
    //   // p_shader->setUniformValue("spec_strength", 0.5f);

    //   plane.draw();
    //   cube.draw();

    //   p_shader->release();
    // }

    QOpenGLShaderProgram *p_shader = this->sp_shader_manager->get("shadow_map_lit_pass")
                                         ->get();

    if (p_shader)
    {
      p_shader->bind();
      p_shader->setUniformValue("model", toQMat(model));
      p_shader->setUniformValue("view", toQMat(view));
      p_shader->setUniformValue("projection", toQMat(projection));
      p_shader->setUniformValue("light_space_matrix", toQMat(light_space_matrix));

      p_shader->setUniformValue("light_pos", toQVec(light_pos));
      p_shader->setUniformValue("view_pos", toQVec(camera_pos));
      p_shader->setUniformValue("shininess", 32.f);
      p_shader->setUniformValue("spec_strength", 1.f);

      this->shadow_depth_texture.bind(0);

      p_shader->setUniformValue("base_color", QVector3D(0.5f, 0.5f, 0.5f));
      plane.draw();

      p_shader->setUniformValue("base_color", QVector3D(0.8f, 0.3f, 0.2f));
      cube.draw();

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

  ImGui::Checkbox("auto_rotate_light", &this->auto_rotate_light);

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
                                 -glm::half_pi<float>(),
                                 glm::half_pi<float>());

      // this->alpha_y = glm::clamp(this->alpha_y,
      //                            -2.f * glm::half_pi<float>(),
      //                            2.f * glm::half_pi<float>());
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
  this->light_theta = 45.f / 180.f * 3.14f;
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
