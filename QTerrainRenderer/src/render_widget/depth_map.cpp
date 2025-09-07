/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/gl_errors.hpp"
#include "qtr/render_widget.hpp"

namespace qtr
{

void RenderWidget::render_depth_map(const glm::mat4 &model,
                                    const glm::mat4 &view,
                                    const glm::mat4 &projection)
{
  QOpenGLShaderProgram *p_shader = this->sp_shader_manager->get("depth_map")->get();

  if (p_shader)
  {
    // backup FBO state to avoid messing up with others FBO (ImGUI
    // for instance...)
    GLint previous_fbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);

    glViewport(0, 0, this->texture_depth.get_width(), this->texture_depth.get_height());
    glBindFramebuffer(GL_FRAMEBUFFER, this->fbo_depth);

    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    // glCullFace(GL_FRONT);

    p_shader->bind();
    p_shader->setUniformValue("model", toQMat(model));
    p_shader->setUniformValue("view", toQMat(view));
    p_shader->setUniformValue("projection", toQMat(projection));

    plane.draw();
    hmap.draw();
    water_plane.draw();
    points_mesh.draw();
    path_mesh.draw();
    instanced_mesh.draw(p_shader);

    p_shader->release();

    // glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    check_gl_error("RenderWidget::render_depth_map: render depth map");

    // set previous FBO back
    glBindFramebuffer(GL_FRAMEBUFFER, previous_fbo);
  }
}

} // namespace qtr
