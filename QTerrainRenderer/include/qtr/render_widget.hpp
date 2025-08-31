/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QElapsedTimer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QTimer>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "nlohmann/json.hpp"

#include "qtr/camera.hpp"
#include "qtr/light.hpp"
#include "qtr/mesh.hpp"
#include "qtr/shader_manager.hpp"
#include "qtr/texture.hpp"

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
  QElapsedTimer        timer;
  bool                 need_update = false;
  bool                 rotating = false;
  bool                 panning = false;
  std::array<float, 2> last_mouse_pos;

  // user parameters
  bool wireframe_mode = false;
  bool auto_rotate_light = false;

  // camera parameters (see reset_camera_position())
  glm::vec3 target;     // Orbit center
  glm::vec2 pan_offset; // Panning offset
  float     distance;   // Zoom (distance to target)
  float     alpha_x;    // Rotation around X (pitch)
  float     alpha_y;    // Rotation around Y (yaw)
  float     fov;
  float     light_phi;   // azimuth
  float     light_theta; // zenith
  float     light_distance = 10.f;

  float scale_h = 0.4f;
  float near_plane = 0.1f;
  float far_plane = 100.f;

  float gamma_correction = 2.f;
  bool  bypass_hmap_texture = false;
  bool  bypass_shadow_map = false;
  float shadow_strength = 0.9f;

  bool apply_tonemap = false;

  // OpenGL
  std::unique_ptr<ShaderManager> sp_shader_manager;
  GLuint                         fbo;

  // TODO clean-up
  Camera camera_shadow_pass;
  Camera camera;
  Light  light;

  Mesh    plane;
  Mesh    hmap;
  Mesh    points_mesh;
  Texture shadow_depth_texture;
  Texture hmap_texture;
};

// some helpers
inline QMatrix4x4 toQMat(const glm::mat4 &m)
{
  return QMatrix4x4(glm::value_ptr(glm::transpose(m)));
}

inline QVector3D toQVec(const glm::vec3 &v) { return QVector3D(v.x, v.y, v.z); }

} // namespace qtr