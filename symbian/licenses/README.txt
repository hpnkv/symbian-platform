The native SIS writer links OpenSSL libcrypto for the historical SHA-1 file
integrity field. OpenSSL 3 is licensed under Apache-2.0; its upstream license
is included in OpenSSL.txt. This repository's Apple Silicon verification used
OpenSSL 3.6.5 from Homebrew and static libcrypto.a linkage. SHA-1 here provides
format compatibility, not package authentication or a signing policy.

No Nokia or EKA2L1 implementation is linked into the platform Python module.
Research oracles compile separately against ignored upstream checkouts, whose
original EPL-1.0 and GPL-3.0-or-later licenses remain in those checkouts.
