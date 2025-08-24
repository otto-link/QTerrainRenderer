/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <QKeyEvent>
#include <QMouseEvent>

#include <imgui.h>

#include "qtr/render_widget.hpp"

namespace qtr
{

void RenderWidget::mousePressEvent(QMouseEvent *e)
{
  ImGuiIO &io = ImGui::GetIO();
  if (e->button() == Qt::LeftButton)
    io.MouseDown[0] = true;
  if (e->button() == Qt::RightButton)
    io.MouseDown[1] = true;
  if (e->button() == Qt::MiddleButton)
    io.MouseDown[2] = true;

  this->need_update = true;
}

void RenderWidget::mouseReleaseEvent(QMouseEvent *e)
{
  ImGuiIO &io = ImGui::GetIO();
  if (e->button() == Qt::LeftButton)
    io.MouseDown[0] = false;
  if (e->button() == Qt::RightButton)
    io.MouseDown[1] = false;
  if (e->button() == Qt::MiddleButton)
    io.MouseDown[2] = false;

  this->need_update = true;
}

void RenderWidget::mouseMoveEvent(QMouseEvent *e)
{
  ImGui::GetIO().MousePos = ImVec2(float(e->position().x()), float(e->position().y()));
  this->need_update = true;
}

void RenderWidget::wheelEvent(QWheelEvent *e)
{
  ImGui::GetIO().MouseWheel += e->angleDelta().y() / 120.0f;
  this->need_update = true;
}

void RenderWidget::keyPressEvent(QKeyEvent *e)
{
  // ImGui::GetIO().KeysDown[e->key()] = true;
  this->need_update = true;
}

void RenderWidget::keyReleaseEvent(QKeyEvent *e)
{
  // ImGui::GetIO().KeysDown[e->key()] = false;
  this->need_update = true;
}

} // namespace qtr
