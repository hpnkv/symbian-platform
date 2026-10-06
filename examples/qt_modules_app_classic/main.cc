#include <QtNetwork/QHostAddress>
#include <QtOpenGL/QGLFormat>
#include <QtSql/QSqlDatabase>
#include <QtWebKit/QWebSettings>
#include <QtXml/QDomDocument>

int main() {
  QHostAddress address(QString::fromLatin1("127.0.0.1"));
  QDomDocument document;
  bool parsed = document.setContent(QString::fromLatin1("<root/>"));
  bool has_drivers = !QSqlDatabase::drivers().isEmpty();
  bool has_web_settings = QWebSettings::globalSettings() != nullptr;
  bool has_gl = QGLFormat::hasOpenGL();
  return address.isNull() || !parsed || !has_drivers || !has_web_settings ||
                 !has_gl;
}
