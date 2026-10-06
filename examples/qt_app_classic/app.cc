#include <QtCore/QByteArray>
#include <QtCore/QSignalMapper>
#include <QtGui/QApplication>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>
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

  QWidget panel;
  panel.setWindowTitle(QString::fromUtf8("QtCore + QtGui"));
  QVBoxLayout layout(&panel);
  QLabel title(QString::fromUtf8("Qt 4.8.1 on Symbian"), &panel);
  QLabel description(
      QString::fromUtf8("QtGui draws this screen. Run creates a QtCore "
                        "QByteArray and checks its contents."),
      &panel);
  description.setWordWrap(true);
  QLabel result(QString::fromUtf8("Tap Run to check QtCore."), &panel);
  result.setWordWrap(true);
  QPushButton run(QString::fromUtf8("Run QtCore check"), &panel);
  QPushButton close(QString::fromUtf8("Close"), &panel);
  layout.addWidget(&title);
  layout.addWidget(&description);
  layout.addWidget(&run);
  layout.addWidget(&result);
  layout.addStretch();
  layout.addWidget(&close);
  QByteArray payload;
  payload.append("808");
  const bool failed = payload != "808" || payload.size() != 3;
  QSignalMapper mapper(&panel);
  mapper.setMapping(&run, failed ? QString::fromUtf8("QByteArray check failed")
                                 : QString::fromUtf8(
                                       "QByteArray contains 808 (3 bytes)"));
  if (!QObject::connect(&run, SIGNAL(clicked()), &mapper, SLOT(map())) ||
      !QObject::connect(&mapper, SIGNAL(mapped(QString)), &result,
                        SLOT(setText(QString)))) {
    return 2;
  }
  // S60 exit animations need Window Server graphics plugins. Disable this
  // connection in the emulator, while keeping the ordinary Qt cleanup path.
  if (!QObject::disconnect(&application, SIGNAL(aboutToQuit()), nullptr,
                           nullptr)) {
    return 3;
  }
  if (!QObject::connect(&close, SIGNAL(clicked()), &application,
                        SLOT(quit()))) {
    return 4;
  }
  panel.showFullScreen();
  const int result_code = application.exec();
  return failed ? 5 : result_code;
}
