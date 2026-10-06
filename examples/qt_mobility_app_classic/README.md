# Qt Mobility 1.0.3 imports

This example constructs an original Qt Mobility contact and location value.
Its `Symbian::QtMobilityContacts` and `Symbian::QtMobilityLocation` targets
provide the public headers, frozen ordinal imports and Qt dependencies. The
source snapshot and package manifest establish Qt Mobility 1.0.3 coverage;
they do not establish which version or plugins a selected Belle firmware has.

Select an installed SDK in ignored `sdk-location.json` as
`{"sdk":"/path/to/native-sdk"}`, configure with
`cmake --preset symbian-pic` and build with
`cmake --build .symbian/build/cmake`. The root SDK project exposes the same
target to IDEs.
