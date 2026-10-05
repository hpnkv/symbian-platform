#include <new>

#include <e32def.h>
#undef __NO_THROW
#define __NO_THROW noexcept
#define __SYMBIAN_STDCPP_SUPPORT__
#define __PLACEMENT_NEW_INLINE
#define __PLACEMENT_VEC_NEW_INLINE
#include <QtGui/QApplication>
#include <QtGui/QPushButton>

static_assert(QT_VERSION == 0x040801);
static_assert(sizeof(void*) == 4 && sizeof(qreal) == 4);

extern "C" int GuiMain() {
  int argc = 3;
  char name[] = "qt_app";
  char style[] = "-style";
  char style_name[] = "plastique";
  char* argv[] = {name, style, style_name, nullptr};
  QApplication::setAttribute(Qt::AA_S60DontConstructApplicationPanes);
  QApplication::setGraphicsSystem(QString::fromUtf8("raster"));
  QApplication application(argc, argv);
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
