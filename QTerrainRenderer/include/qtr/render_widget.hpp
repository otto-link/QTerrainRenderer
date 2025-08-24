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

  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;

  QSize sizeHint() const override;

protected:
  void initializeGL() override;
  void resizeEvent(QResizeEvent *event) override;
  void resizeGL(int w, int h) override;

private:
  std::string title;
  QTimer      frame_timer;
};

} // namespace qtr