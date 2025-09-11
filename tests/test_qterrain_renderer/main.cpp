/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <QApplication>

#include "qterrain_renderer.hpp"

int main(int argc, char *argv[])
{
  QTR_LOG->info("testing qterrain_renderer...");

  qputenv("QT_LOGGING_RULES", QTR_QPUTENV_QT_LOGGING_RULES);
  QApplication app(argc, argv);

  qtr::RenderWidget *renderer = new qtr::RenderWidget("Test widget");

  renderer->show();

  // set the heightmap data
  {
    int                width, height;
    std::vector<float> data = qtr::load_png_as_grayscale("hmap.png", width, height);

    renderer->set_heightmap_geometry(data, width, height);
  }

  {
    int                  width, height;
    std::vector<uint8_t> data = qtr::load_png_as_8bit_rgba("texture.png", width, height);

    renderer->set_texture_albedo(data, width);
    // renderer->reset_texture_albedo();
  }

  {
    int                  width, height;
    std::vector<uint8_t> data = qtr::load_png_as_8bit_rgba("nmap2.png", width, height);

    renderer->set_texture_normal(data, width);
    renderer->reset_texture_normal();
  }

  {
    std::vector<float> x, y, h;
    x = {0.05f, 0.1f, 0.2f, 0.7f, 0.8f};
    y = {0.2f, 0.2f, 0.4f, 0.7f, 0.8f};
    h = {0.8f, 0.2f, 1.f, 0.5f, 0.7f};

    renderer->set_points(x, y, h);
    // renderer->reset_points();
  }

  {
    std::vector<float> x, y, h;
    x = {0.05f, 0.1f, 0.2f, 0.7f, 0.8f};
    y = {0.2f, 0.2f, 0.4f, 0.7f, 0.8f};
    h = {0.8f, 0.2f, 1.f, 0.5f, 0.7f};

    renderer->set_path(x, y, h);
    // renderer->reset_points();
  }

  return app.exec();
}
