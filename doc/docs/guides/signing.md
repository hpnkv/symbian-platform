# Signing identities and applications

Open **Symbian Console → Signing** to browse locally held identities, including
certificate subjects, SHA-256 fingerprints and expiry dates. Choose **Create
signing identity** for a self-signed RSA identity, or **Import signing identity**
to copy an existing PEM certificate and matching unencrypted RSA private key.
Names contain letters, digits, underscores or hyphens and start with a letter
or digit. Existing identities are never replaced.

The default store is `$XDG_DATA_HOME/symbian/signing`, or
`~/.local/share/symbian/signing`. Directories are private to the host user and
keys have mode `0600`. Keep identities and private keys outside source control.
OpenSSL is required for identity creation and certificate checks. The GUI's
view options can select another identity folder.

```sh
uv run symbian signing create --identity developer --common-name "Developer"
uv run symbian signing list
uv run symbian signing import --identity imported \
  --certificate /private/certificate.pem --private-key /private/key.pem
```

To sign an application, first create its SIS with **Applications → Package an
application** or `symbian package`. Then choose **Signing → Sign an application**,
select an identity, browse to the unsigned SIS and choose a new output filename.
The native signer validates the supported SIS profile and verifies the resulting
signature before writing output. Existing output files are never overwritten.
Already signed or unsupported packages are rejected by the native signer.

```sh
uv run symbian signing sign .symbian/package/my_app.sis \
  --identity developer --destination .symbian/package/my_app-signed.sis
```

**Archive signing identity** removes an identity from the active list while
retaining its certificate and key in the store's private `.archive/` directory:

```sh
uv run symbian signing archive --identity developer
```

Self-signing does not establish phone trust or grant restricted capabilities.
Certificate acceptance and installation remain subject to the phone's policy.
