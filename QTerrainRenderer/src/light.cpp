/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <glm/gtc/matrix_transform.hpp>

#include "qtr/light.hpp"
#include "qtr/logger.hpp"

namespace qtr
{

glm::vec3 Light::get_dir() const { return normalize(this->target - this->position); }

void Light::set_position_spherical(float distance, float theta, float phi)
{
  this->position.x = distance * std::cos(theta) * std::sin(phi);
  this->position.y = distance * std::sin(theta);
  this->position.z = distance * std::cos(theta) * std::cos(phi);
}

} // namespace qtr
