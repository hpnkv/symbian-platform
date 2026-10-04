# Public test identity

These PEM files are a generated test-only self-signed P-256 certificate and its
publicly committed private key. Never use them for a deployed service or trust
store. The certificate has DNS name `sdk-test`; its fixed validity is visible
with `openssl x509 -in server-cert.pem -noout -dates`.

Generated locally for this fork using OpenSSL `req -x509`, prime256v1, a 3,650-day
validity and `subjectAltName=DNS:sdk-test`. Tests load the files from memory; the
library has no file-I/O dependency. The host handshake tests exercise required
certificate and hostname verification. They are not a guest trust-store policy.
