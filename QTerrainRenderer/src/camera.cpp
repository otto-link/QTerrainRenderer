/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <glm/gtc/matrix_transform.hpp>

#include "qtr/camera.hpp"
#include "qtr/logger.hpp"

namespace qtr
{

glm::mat4 Camera::get_projection_matrix_ortho(float ortho_size) const
{
  return glm::ortho(-ortho_size,
                    ortho_size,
                    -ortho_size,
                    ortho_size,
                    this->near_plane,
                    this->far_plane);
}

glm::mat4 Camera::get_projection_matrix_perspective(float aspect_ratio) const
{
  return glm::perspective(this->fov, aspect_ratio, this->near_plane, this->far_plane);
}

glm::mat4 Camera::get_view_matrix() const
{
  return glm::lookAt(this->position, this->target, this->up);
}

void Camera::set_position_angles(float distance, float alpha_x, float alpha_y)
{
  this->position.x = distance * std::cos(alpha_x) * std::sin(alpha_y);
  this->position.y = distance * std::sin(alpha_x);
  this->position.z = distance * std::cos(alpha_x) * std::cos(alpha_y);
}

} // namespace qtr
