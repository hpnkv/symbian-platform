#include <QtContacts/QContact>
#include <QtLocation/QGeoCoordinate>

int main() {
  QtMobility::QContact contact;
  QtMobility::QGeoCoordinate coordinate(0.0, 0.0);
  return contact.isEmpty() && coordinate.isValid() ? 0 : 1;
}
