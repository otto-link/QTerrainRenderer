/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/imgui_widgets.hpp"
#include "qtr/logger.hpp"

// TODO tmp

namespace qtr
{

bool imgui_show_water_preset_selector(glm::vec3 &shallow, glm::vec3 &deep)
{
  bool ret = false;

  // Build list of preset names
  static std::vector<const char *> preset_names;
  if (preset_names.empty())
  {
    for (auto &kv : water_colors)
      preset_names.push_back(kv.first.c_str());
  }

  // Find current index
  int current_index = 0;
  for (size_t i = 0; i < preset_names.size(); i++)
  {
    if (current_water_preset == preset_names[i])
    {
      current_index = static_cast<int>(i);
      break;
    }
  }

  // Combo box
  if (ImGui::Combo("Water Preset",
                   &current_index,
                   preset_names.data(),
                   (int)preset_names.size()))
  {
    // Update selected preset
    current_water_preset = preset_names[current_index];
    ret = true;
  }

  // Display colors of the selected preset
  auto &preset = water_colors[current_water_preset];
  shallow = preset.first;
  deep = preset.second;

  ImGui::Indent();

  ImGui::Text("Shallow color:");
  ImGui::ColorButton("##shallow_color",
                     ImVec4(shallow.r, shallow.g, shallow.b, 1.0f),
                     0,
                     ImVec2(50, 20));
  ImGui::SameLine();
  ImGui::Text("(%.2f, %.2f, %.2f)", shallow.r, shallow.g, shallow.b);

  ImGui::Text("Deep color:");
  ImGui::ColorButton("##deep_color",
                     ImVec4(deep.r, deep.g, deep.b, 1.0f),
                     0,
                     ImVec2(50, 20));
  ImGui::SameLine();
  ImGui::Text("(%.2f, %.2f, %.2f)", deep.r, deep.g, deep.b);

  ImGui::Unindent();

  return ret;
}

} // namespace qtr
