# Source provenance

- Symbian port: https://github.com/shinovon/mbedtls-symbian
- Fork point: `625ede204b374ba2091dd6f43a91e986c23c6fbd`
  (2026-09-07, “Fix snprintf implicit declarations”).
- Inherited Mbed TLS version: 3.4.1.
- Original upstream: https://github.com/Mbed-TLS/mbedtls
- License: Apache-2.0; see `LICENSE` and individual source notices.

The full original Git history is retained. The read-only upstream remote is named
`upstream`; this local fork has no publication remote. SDK integration changes
are committed separately from imported source history.

The historical system wrapper is a separate project,
https://github.com/shinovon/symbian-tls. It has not been imported. Its system DLL
identity, capabilities and installation patches are not this library’s deployment
model. Historical frozen exports and bundled CA data are retained as reference
material, not installed or silently selected as the new trust policy.

Local SDK dependency: the uncommitted implementation in `~/dev/symbian`.
Tests must record the materialized SDK digests because Git HEAD alone does not
identify that implementation. A project-local, ignored SDK selection makes a
future installed SDK interchangeable without modifying shared project files.
