/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QTimer>

#include <glm/glm.hpp>

#include "nlohmann/json.hpp"

#include "qtr/mesh.hpp"
#include "qtr/shader_manager.hpp"

namespace qtr
{

class RenderWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
  Q_OBJECT

public:
  explicit RenderWidget(const std::string &_title = "", QWidget *parent = nullptr);
  ~RenderWidget();

  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;

  QSize sizeHint() const override;

protected:
  void initializeGL() override;
  void paintGL() override;
  void resizeEvent(QResizeEvent *event) override;
  void resizeGL(int w, int h) override;

  // --- Input forwarding to ImGui
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void keyReleaseEvent(QKeyEvent *e) override;

private:
  void reset_camera_position();

  // --- Members
  std::string title;

  // GUI
  QTimer               frame_timer;
  bool                 need_update = false;
  bool                 rotating = false;
  bool                 panning = false;
  std::array<float, 2> last_mouse_pos;

  // user parameters
  bool wireframe_mode = false;

  // camera parameters
  glm::vec3 target;     // Orbit center
  glm::vec2 pan_offset; // Panning offset
  float     distance;   // Zoom (distance to target)
  float     alpha_x;    // Rotation around X (pitch)
  float     alpha_y;    // Rotation around Y (yaw)

  float scale_h = 0.4f;
  float fov = 45.f;
  float near_plane = 0.1f;
  float far_plane = 100.f;

  // OpenGL
  std::unique_ptr<ShaderManager> sp_shader_manager;

  // TODO DBG
  Mesh cube;
};

} // namespace qtr