/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include "qtr/windows_patch.hpp"

#include <map>
#include <string>

#include <glm/vec3.hpp>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD  
#include <imgui.h>

#include "qtr/water_colors.hpp"

namespace qtr
{

static std::string current_water_preset = "caribbean"; // default

bool imgui_show_water_preset_selector(glm::vec3 &shallow, glm::vec3 &deep);

void imgui_set_blender_style();

} // namespace qtr