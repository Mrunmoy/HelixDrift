# Dev signing key — the .pem is committed intentionally (CI-only dev key).
# For production, replace dev_signing_key.pem with a secret stored outside the repo.

## Regenerating the dev key

```
imgtool keygen --key keys/dev_signing_key.pem --type ed25519
imgtool getpub --key keys/dev_signing_key.pem   # paste output into bootloader/port/ed25519_pub_key.h
```
