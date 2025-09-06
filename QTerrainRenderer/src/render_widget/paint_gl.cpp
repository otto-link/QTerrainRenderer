/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <stdexcept>

#include <QOpenGLFunctions>

#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include "qtr/config.hpp"
#include "qtr/gl_errors.hpp"
#include "qtr/imgui_widgets.hpp"
#include "qtr/logger.hpp"
#include "qtr/mesh.hpp"
#include "qtr/primitives.hpp"
#include "qtr/render_widget.hpp"
#include "qtr/utils.hpp"

namespace qtr
{

void RenderWidget::paintGL()
{
  this->update_time();

  this->update_light();
  this->update_camera();

  this->render_scene();
  this->render_ui(); // ImGUI overlay

  // check for errors...
  check_gl_error("RenderWidget::paintGL");
}

void RenderWidget::render_scene()
{
  // model
  glm::mat4 model = glm::mat4(1.0f);
  model = glm::scale(model, glm::vec3(1.f, this->scale_h, 1.f));

  // shadow map
  glm::mat4 light_space_matrix;
  this->render_shadow_map(model, light_space_matrix);

  // projection
  float aspect_ratio = static_cast<float>(this->width()) /
                       static_cast<float>(this->height());

  glm::mat4 projection = this->camera.get_projection_matrix_perspective(aspect_ratio);

  // depth map
  this->render_depth_map(model, this->camera.get_view_matrix(), projection);

  // --- main lit pass

  this->setup_gl_state();

  QOpenGLShaderProgram *p_shader = this->sp_shader_manager->get("shadow_map_lit_pass")
                                       ->get();

  if (p_shader)
  {
    p_shader->bind();

    this->set_common_uniforms(*p_shader,
                              model,
                              projection,
                              this->camera.get_view_matrix(),
                              light_space_matrix);

    // base plane
    if (this->render_plane)
    {
      p_shader->setUniformValue("base_color", QVector3D(0.5f, 0.5f, 0.5f));
      p_shader->setUniformValue("add_ambiant_occlusion", false);
      plane.draw();
    }

    // points
    if (this->render_points)
    {
      p_shader->setUniformValue("base_color", QVector3D(0.f, 1.f, 0.f));
      p_shader->setUniformValue("add_ambiant_occlusion", false);
      points_mesh.draw();
    }

    // path
    if (this->render_path)
    {
      p_shader->setUniformValue("base_color", QVector3D(1.f, 0.f, 1.f));
      p_shader->setUniformValue("add_ambiant_occlusion", false);
      path_mesh.draw();
    }

    // heightmap
    if (this->render_hmap)
    {
      p_shader->setUniformValue("base_color", QVector3D(0.8f, 0.8f, 0.8f));
      p_shader->setUniformValue("use_texture_albedo",
                                true && !this->bypass_texture_albedo &&
                                    this->texture_albedo.is_active());

      if (this->texture_normal.is_active())
        p_shader->setUniformValue("normal_map_scaling", this->normal_map_scaling);

      p_shader->setUniformValue("add_ambiant_occlusion", this->add_ambiant_occlusion);
      hmap.draw();

      p_shader->setUniformValue("normal_map_scaling", 0.f);
    }

    if (this->add_water)
    {
      p_shader->setUniformValue("spec_strength", this->water_spec_strength);

      p_shader->setUniformValue("add_ambiant_occlusion", false);
      p_shader->setUniformValue("use_texture_albedo", false);

      p_shader->setUniformValue("use_water_colors", true);

      p_shader->setUniformValue("color_shallow_water", toQVec(this->color_shallow_water));
      p_shader->setUniformValue("color_deep_water", toQVec(this->color_deep_water));

      p_shader->setUniformValue("water_color_depth", this->water_color_depth);

      p_shader->setUniformValue("add_water_foam", this->add_water_foam);
      p_shader->setUniformValue("foam_color", toQVec(this->foam_color));
      p_shader->setUniformValue("foam_depth", this->foam_depth);

      p_shader->setUniformValue("add_water_waves", this->add_water_waves);
      p_shader->setUniformValue("angle_spread_ratio", this->angle_spread_ratio);
      p_shader->setUniformValue("waves_alpha", this->waves_alpha);
      p_shader->setUniformValue("waves_kw", this->waves_kw);
      p_shader->setUniformValue("waves_amplitude", this->waves_amplitude);
      p_shader->setUniformValue("waves_normal_amplitude", this->waves_normal_amplitude);

      if (this->animate_waves)
        p_shader->setUniformValue("waves_speed", this->waves_speed);
      else
        p_shader->setUniformValue("waves_speed", 0.f);

      // either use the input mesh or use a simple plane surface as a
      // fallback
      if (this->water_mesh.is_active())
        this->water_mesh.draw();
      else
        this->water_plane.draw();
    }

    this->unbind_textures();

    p_shader->release();
  }
}

void RenderWidget::render_ui()
{
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

  ret |= ImGui::Checkbox("Normal visualization", &this->normal_visualization);
  ret |= ImGui::Checkbox("Render plane", &this->render_plane);
  ret |= ImGui::Checkbox("Render points", &this->render_points);
  ret |= ImGui::Checkbox("Render path", &this->render_path);
  ret |= ImGui::Checkbox("Render terrain", &this->render_hmap);
  ret |= ImGui::Checkbox("Wireframe mode", &this->wireframe_mode);
  ret |= ImGui::SliderFloat("scale_h", &this->scale_h, 0.f, 2.f);
  ret |= ImGui::SliderAngle("FOV", &this->fov, 10.f, 180.f);

  ImGui::Text("Normal map");
  ret |= ImGui::SliderFloat("Normal map scaling", &this->normal_map_scaling, 0.f, 2.f);

  ImGui::Text("Light");
  ret |= ImGui::SliderAngle("Azimuth", &this->light_phi, -180.f, 180.f);
  ret |= ImGui::SliderAngle("Zenith", &this->light_theta, 0.f, 90.f);
  ImGui::Checkbox("Auto rotate light", &this->auto_rotate_light);

  ImGui::Text("Shadow map");
  ret |= ImGui::Checkbox("Bypass shadow map", &this->bypass_shadow_map);
  ret |= ImGui::SliderFloat("Shadow strength", &this->shadow_strength, 0.f, 1.f);

  ImGui::Text("Ambiant occlusion");
  ret |= ImGui::Checkbox("Add ambiant occlusion", &this->add_ambiant_occlusion);
  ret |= ImGui::SliderFloat("Ambiant occlusion strength",
                            &this->ambiant_occlusion_strength,
                            0.f,
                            1000.f);
  ret |= ImGui::SliderInt("Ambiant occlusion radius",
                          &this->ambiant_occlusion_radius,
                          0,
                          32);

  if (ImGui::Button("Reset view"))
  {
    this->reset_camera_position();
    this->need_update = true;
  }

  ImGui::Text("Albedo");
  ret |= ImGui::SliderFloat("Gamma correction", &this->gamma_correction, 0.01f, 4.f);
  ret |= ImGui::Checkbox("Bypass albedo texture", &this->bypass_texture_albedo);
  ret |= ImGui::Checkbox("Apply tonemap", &this->apply_tonemap);

  ImGui::SeparatorText("Water");

  {
    ret |= ImGui::Checkbox("Add water", &this->add_water);

    if (ImGui::SliderFloat("Water elevation", &this->water_elevation, 0.f, 1.f))
    {
      ret = true;
      generate_plane(this->water_plane,
                     0.f,
                     this->water_elevation,
                     0.f,
                     2.f,
                     2.f); // TODO hardcoded
    }

    ret |= ImGui::SliderFloat("Water color depth", &this->water_color_depth, 0.f, 0.2f);
    ret |= ImGui::SliderFloat("Water specularity", &this->water_spec_strength, 0.f, 1.f);

    ret |= imgui_show_water_preset_selector(this->color_shallow_water,
                                            this->color_deep_water);

    ret |= ImGui::Checkbox("Add foam", &this->add_water_foam);
    ret |= ImGui::SliderFloat("Foam depth", &this->foam_depth, 0.f, 0.1f);

    ret |= ImGui::Checkbox("Add waves", &this->add_water_waves);
    ret |= ImGui::SliderFloat("Waves wavenumber", &this->waves_kw, 0.f, 2048.f);
    ret |= ImGui::SliderFloat("Waves amplitude", &this->waves_amplitude, 0.f, 0.1f);
    ret |= ImGui::SliderFloat("Waves normal amplitude",
                              &this->waves_normal_amplitude,
                              0.f,
                              0.1f);
    ret |= ImGui::SliderAngle("Waves angle", &this->waves_alpha, -180.f, 180.f);
    ret |= ImGui::SliderFloat("angle_spread_ratio", &this->angle_spread_ratio, 0.f, 0.1f);

    ret |= ImGui::Checkbox("Animate waves", &this->animate_waves);
    ret |= ImGui::SliderFloat("Waves speed", &this->waves_speed, 0.f, 1.f);

    // force update if animation requested
    if (this->animate_waves)
      this->need_update = true;
  }

  ImGui::SeparatorText("Fog##sep");

  {
    ret |= ImGui::Checkbox("Fog", &this->add_fog);
  }

  ImGui::SeparatorText("Atmospheric scattering##sep");

  {
    ret |= ImGui::Checkbox("Atmospheric scattering", &this->add_atmospheric_scattering);
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
                                 -0.99f * glm::half_pi<float>(),
                                 0.99f * glm::half_pi<float>());
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
}

} // namespace qtr
