#include <QtNetwork/QHostAddress>
#include <QtOpenGL/QGLFormat>
#include <QtSql/QSqlDatabase>
#include <QtWebKit/QWebSettings>
#include <QtXml/QDomDocument>

#include "ui.h"

int RunFeature(void* absl_nullable) {
  const QHostAddress address(QString::fromLatin1("127.0.0.1"));
  QDomDocument document;
  const bool parsed = document.setContent(QString::fromLatin1("<root/>"));
  const bool has_drivers = !QSqlDatabase::drivers().isEmpty();
  const bool has_web_settings = QWebSettings::globalSettings() != nullptr;
  const bool has_gl = QGLFormat::hasOpenGL();
  return address.isNull() || !parsed || !has_drivers || !has_web_settings ||
         !has_gl;
}

int main() {
  return classic_demo_ui::Show(_L("QT MODULES"),
                               _L("Queries Network, XML and SQL."),
                               _L("Checks WebKit and OpenGL."),
                               _L("QT MODULES CHECKED"), &RunFeature, nullptr);
}
