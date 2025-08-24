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

  return app.exec();
}
