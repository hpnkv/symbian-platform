#include <QtContacts/QContact>
#include <QtLocation/QGeoCoordinate>

#include "ui.h"

int RunFeature(void* absl_nullable) {
  QtMobility::QContact contact;
  QtMobility::QGeoCoordinate coordinate(0.0, 0.0);
  return contact.isEmpty() && coordinate.isValid() ? 0 : 1;
}

int main() {
  return classic_demo_ui::Show(
      _L("QT MOBILITY"), _L("Creates a contact and place."),
      _L("Checks the coordinates."), _L("CONTACT + PLACE CHECKED"), &RunFeature,
      nullptr);
}
