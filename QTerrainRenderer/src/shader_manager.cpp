/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "qtr/shader_manager.hpp"
#include "qtr/logger.hpp"

namespace qtr
{

ShaderManager::~ShaderManager()
{
  QTR_LOG->trace("ShaderManager::~ShaderManager");
  this->clear();
}

bool ShaderManager::add_shader_from_code(const std::string &name,
                                         const std::string &vertex_code,
                                         const std::string &fragment_code)
{
  QTR_LOG->trace("ShaderManager::add_shader_from_code: {}", name);

  auto shader = std::make_unique<Shader>();

  if (!shader->from_code(vertex_code, fragment_code))
  {
    QTR_LOG->error("ShaderManager::add_shader_from_code: could not add shader");
    return false;
  }

  this->shaders[name] = std::move(shader);
  return true;
}

bool ShaderManager::add_shader_from_file(const std::string &name,
                                         const std::string &vertex_path,
                                         const std::string &fragment_path)
{
  QTR_LOG->trace("ShaderManager::add_shader_from_file: {}", name);

  auto shader = std::make_unique<Shader>();

  if (!shader->from_file(vertex_path, fragment_path))
  {
    QTR_LOG->error("ShaderManager::add_shader_from_file: could not add shader");
    return false;
  }

  this->shaders[name] = std::move(shader);
  return true;
}

Shader *ShaderManager::get(const std::string &name)
{
  auto it = this->shaders.find(name);
  return (it != this->shaders.end()) ? it->second.get() : nullptr;
}

void ShaderManager::clear() { this->shaders.clear(); }

} // namespace qtr
