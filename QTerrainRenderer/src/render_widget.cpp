/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <QOpenGLFunctions>

#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "qtr/config.hpp"
#include "qtr/logger.hpp"
#include "qtr/mesh.hpp"
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
  this->sp_shader_manager->add_shader_from_code("base", vertex_normal, fragment_normal);

  this->cube.create(cube_vertices, cube_indices);

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
  // --- FIRST - OpenGL scene rendering

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

  // OpenGL rendering

  // TODO DBG
  // cube.create(face_vertices, face_indices); //, cube_indices);

  {
    // transformation matrices

    // MODEL
    glm::mat4 model = glm::mat4(1.f);
    glm::vec3 position(this->dx, this->dy, 0.f);
    model = glm::translate(model, position);

    model = glm::rotate(model, this->alpha_y, glm::vec3(0.f, 1.f, 0.f));
    model = glm::rotate(model, this->alpha_x, glm::vec3(1.f, 0.f, 0.f));

    glm::vec3 scale_mat(this->scale, this->scale * this->scale_h, this->scale);
    model = glm::scale(model, scale_mat);

    // VIEW
    glm::vec3 camera_pos = glm::vec3(5.f, 1.f, 5.f);
    glm::vec3 camera_target = glm::vec3(0.f, 0.f, 0.f);
    glm::vec3 up_vector = glm::vec3(0.f, 1.f, 0.f);
    glm::mat4 view = glm::lookAt(camera_pos, camera_target, up_vector);

    // PROJECTION
    float aspect_ratio = static_cast<float>(this->width()) /
                         static_cast<float>(this->height());

    glm::mat4 projection = glm::perspective(glm::radians(this->fov),
                                            aspect_ratio,
                                            this->near_plane,
                                            this->far_plane);

    // RENDER

    QOpenGLShaderProgram *p_shader = this->sp_shader_manager->get("base")->get();

    if (p_shader)
    {
      p_shader->bind();
      p_shader->setUniformValue("model",
                                QMatrix4x4(glm::value_ptr(glm::transpose(model))));
      p_shader->setUniformValue("view", QMatrix4x4(glm::value_ptr(glm::transpose(view))));
      p_shader->setUniformValue("projection",
                                QMatrix4x4(glm::value_ptr(glm::transpose(projection))));

      p_shader->setUniformValue("color", QVector3D(0.8f, 0.3f, 0.2f));
      p_shader->setUniformValue("light_dir", QVector3D(0.5f, 2.0f, 0.3f));

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

  ImGui::Checkbox("Wireframe mode", &this->wireframe_mode);
  ImGui::SliderAngle("alpha_x", &this->alpha_x);
  ImGui::SliderAngle("alpha_y", &this->alpha_y);
  ImGui::SliderFloat("scale", &this->scale, 0.f, 10.f);
  ImGui::SliderFloat("scale_h", &this->scale_h, 0.f, 1.f);

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
