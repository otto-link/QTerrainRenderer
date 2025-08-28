/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QTimer>

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
  // --- Members
  std::string title;

  // GUI
  QTimer frame_timer;
  bool   need_update = false;

  // user parameters
  bool wireframe_mode = false;

  float dx = 0.f;
  float dy = 0.f;
  float alpha_x = 35.f / 180.f * 3.14f;
  float alpha_y = -25.f / 180.f * 3.14f;
  float scale = 0.7f;
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