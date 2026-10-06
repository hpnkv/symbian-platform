#include <QtCore/QByteArray>
#include <QtGui/QApplication>
#include <QtGui/QPushButton>
#include <absl/base/nullability.h>

static_assert(QT_VERSION == 0x040801);
static_assert(sizeof(void*) == 4 && sizeof(qreal) == 4);

int main() {
  int argc = 3;
  char name[] = "qt_app_classic";
  char style[] = "-style";
  char style_name[] = "plastique";
  char* absl_nullable argv[] = {name, style, style_name, nullptr};
  QApplication::setAttribute(Qt::AA_S60DontConstructApplicationPanes);
  QApplication::setGraphicsSystem(QString::fromUtf8("raster"));
  QApplication application(argc, argv);
  // Exercise the complete QtCore target, including exported shared-null data.
  QByteArray payload;
  payload.append("808");
  if (payload != "808") {
    return 4;
  }
  QPushButton button(QString::fromUtf8("Hello from Symbian Qt\nTap to close"));
  button.setWindowTitle(QString::fromUtf8("Symbian Qt"));
  button.resize(280, 100);
  // S60 exit animations need Window Server graphics plugins. Disable this
  // connection in the emulator, while keeping the ordinary Qt cleanup path.
  if (!QObject::disconnect(&application, SIGNAL(aboutToQuit()), nullptr,
                           nullptr)) {
    return 2;
  }
  if (!QObject::connect(&button, SIGNAL(clicked()), &application,
                        SLOT(quit()))) {
    return 3;
  }
  button.showFullScreen();
  return application.exec();
}
