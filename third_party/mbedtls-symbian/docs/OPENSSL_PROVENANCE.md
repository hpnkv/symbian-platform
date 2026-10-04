# Qt TLS source provenance

Vendored original source: https://github.com/shinovon/openssl-symbian,
revision `deda3534c1fe473cfbd73b4fb9e95fe97320b7bd`. All 1,346 tracked files are
copied unchanged under `third_party/openssl-symbian`; `UPSTREAM.json` records
exact SHA-256 identities. This is the author’s actual OpenSSL 1.0.2u port,
not an Mbed TLS compatibility shim.

Original OpenSSL and SSLeay license terms are preserved in that directory’s
`LICENSE` and source notices. They are independent of this repository’s
Apache-2.0 Mbed TLS/adapter license. Required acknowledgments:

This product includes software developed by the OpenSSL Project for use in the
OpenSSL Toolkit (http://www.openssl.org/).
This product includes cryptographic software written by Eric Young
(eay@cryptsoft.com). This product includes software written by Tim Hudson
(tjh@cryptsoft.com).

SDK build adaptations live outside the source snapshot under `compat/openssl`.
Historical MMP/SIS/frozen exports remain evidence; the new build consumes an
explicit CMake source inventory. Changes to the snapshot require a separate
upstream update with an updated identity manifest.

Qt’s historical OpenSSL API/ABI prevents substituting Mbed TLS directly. Keep
Mbed TLS for new applications and this separate OpenSSL compatibility target
for legacy consumers. OpenSSL 1.0.2u supplies TLS 1.2, not TLS 1.3. Building this
pin does not imply current security maintenance or tested Qt/device compatibility.
