/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <QOpenGLFunctions>

#include "qtr/config.hpp"
#include "qtr/logger.hpp"
#include "qtr/render_widget.hpp"

namespace qtr
{

RenderWidget::RenderWidget(const std::string &_title, QWidget *parent)
    : QOpenGLWidget(parent), QOpenGLFunctions(), title(_title)
{
  QTR_LOG->trace("RenderWidget::RenderWidget");

  this->setWindowTitle(this->title.c_str());
  this->setFocusPolicy(Qt::StrongFocus);
  this->setMouseTracking(true);

  // force 60 FPS update
  this->connect(&frame_timer,
                &QTimer::timeout,
                this,
                QOverload<>::of(&RenderWidget::update));
  this->frame_timer.start(16);
}

void RenderWidget::initializeGL()
{
  QTR_LOG->trace("RenderWidget::initializeGL");

  QOpenGLFunctions::initializeOpenGLFunctions();
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

void RenderWidget::resizeEvent(QResizeEvent *event)
{
  QOpenGLWidget::resizeEvent(event);

  // TODO update buffers
  this->repaint();
}

void RenderWidget::resizeGL(int w, int h)
{
  this->glViewport(0, 0, w, h);
  this->repaint();
}

QSize RenderWidget::sizeHint() const { return QSize(QTR_CONFIG->widget.size_hint); }

} // namespace qtr
