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

  void set_heightmap_geometry(const std::vector<float> &data,
                              int                       width,
                              int                       height,
                              bool                      add_skirt = true);
  void reset_heightmap_geometry();

  // RGBA 8bit
  void set_texture_albedo(const std::vector<uint8_t> &data, int width);
  void reset_texture_albedo();

  // RGB 8bit
  void set_texture_normal(const std::vector<uint8_t> &data, int width);
  void reset_texture_normal();

protected:
  void initializeGL() override;
  void resizeEvent(QResizeEvent *event) override;
  void resizeGL(int w, int h) override;

  // --- OpenGL rendering
  void paintGL() override;
  void render_shadow_map(const glm::mat4 &model,
                         const glm::mat4 &view,
                         const glm::mat4 &projection);
  void render_shadow_map(const glm::mat4 &model, glm::mat4 &light_space_matrix);

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

  QElapsedTimer timer;
  float         time = 0.f;

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

  float scale_h = 1.0f;
  float near_plane = 0.1f;
  float far_plane = 100.f;

  bool normal_visualization = false;

  float normal_map_scaling = 1.f;

  float gamma_correction = 2.f;
  bool  bypass_texture_albedo = false;
  bool  bypass_shadow_map = false;
  float shadow_strength = 0.9f;
  bool  add_ambiant_occlusion = false;
  float ambiant_occlusion_strength = 5.f;
  int   ambiant_occlusion_radius = 3;

  bool apply_tonemap = false;

  // water
  bool      add_water = true;
  float     water_elevation = 0.05f;
  glm::vec3 color_shallow_water;
  glm::vec3 color_deep_water;
  float     water_color_depth = 0.015f;
  float     water_spec_strength = 0.5f;

  bool      add_water_foam = true;
  glm::vec3 foam_color = glm::vec3(1.f, 1.f, 1.f);
  float     foam_depth = 0.005f;

  bool  add_water_waves = true;
  float angle_spread_ratio = 0.f;
  float waves_alpha = 30.f / 180.f * 3.14f;
  float waves_kw = 256.f;
  float waves_amplitude = 0.005f;
  float waves_normal_amplitude = 0.02f;
  bool  animate_waves = false;
  float waves_speed = 0.2f;

  // --- Fog
  bool add_fog = false;

  // --- Atmospheric scattering
  bool add_atmospheric_scattering = false;

  // OpenGL
  std::unique_ptr<ShaderManager> sp_shader_manager;
  GLuint                         fbo;
  GLuint                         fbo_depth;

  // TODO clean-up
  Camera camera_shadow_pass;
  Camera camera;
  Light  light;

  Mesh plane;
  Mesh hmap;
  Mesh water_plane;
  Mesh points_mesh;

  Texture texture_albedo;
  Texture texture_hmap;
  Texture texture_normal; // for details
  Texture texture_shadow_map;
  Texture texture_depth;
};

// some helpers
inline QMatrix4x4 toQMat(const glm::mat4 &m)
{
  return QMatrix4x4(glm::value_ptr(glm::transpose(m)));
}

inline QVector3D toQVec(const glm::vec3 &v) { return QVector3D(v.x, v.y, v.z); }
inline QVector2D toQVec(const glm::vec2 &v) { return QVector2D(v.x, v.y); }

} // namespace qtr