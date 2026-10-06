# Qt module imports

This small application links the original guest QtNetwork, QtSql, QtXml,
QtWebKit and QtOpenGL interfaces through `Symbian::QtWebKit`, `Symbian::QtSql`
and `Symbian::QtXml`. The QtWebKit target supplies its recorded transitive
Qt dependencies. Running the application exercises the selected modules and
returns a failure if a required runtime service is unavailable. Building
this example establishes header and import
compatibility, not that every Qt plugin or service exists in a chosen firmware.

Create an ignored `sdk-location.json` containing `{"sdk":"/path/to/native-sdk"}`,
then configure with `cmake --preset symbian-pic` and build with
`cmake --build .symbian/build/cmake`. The root SDK project also exposes the
target for IDE analysis.
