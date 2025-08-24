/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QTimer>

#include "nlohmann/json.hpp"

namespace qtr
{

class RenderWidget : public QOpenGLWidget, protected QOpenGLFunctions
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
  std::string title;

  QTimer frame_timer;
  bool   need_update = false;

  // DBG
  float a = 10.f;
};

} // namespace qtr