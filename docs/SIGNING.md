# Signing the installers

Unsigned, Windows shows a SmartScreen warning that most buyers read as "this is
malware", and macOS refuses the package outright unless the user knows to
right-click and Open. Signing removes both. It also stops the plugin being
quarantined on first load by a host.

Two separate problems: signing proves who built it, and on macOS notarisation
proves Apple has scanned it. You need both there, and only the first on Windows.

---

## macOS

### What to get

An **Apple Developer Program** membership, then two certificates from the
developer portal — they are not interchangeable:

| Certificate | Signs |
| --- | --- |
| Developer ID **Application** | `Click.vst3`, `Click.component`, `Click.app` |
| Developer ID **Installer** | the `.pkg` |

Plus an App Store Connect API key (Issuer ID, Key ID, and a `.p8` file) for
notarisation. An Apple ID with an app-specific password also works, but an API
key is easier to keep in CI.

### Signing

`build-installer.sh` does both signing steps when the identities are in the
environment. It signs the *staged copies*, so the CMake output is untouched:

```bash
export CLICK_CODESIGN_IDENTITY="Developer ID Application: Fuzzy Audio LLC (TEAMID)"
export CLICK_INSTALLER_IDENTITY="Developer ID Installer: Fuzzy Audio LLC (TEAMID)"
packaging/macos/build-installer.sh
```

`security find-identity -v -p codesigning` lists the exact strings. The bundles
are signed with `--options runtime` (the hardened runtime) because the notary
service rejects anything without it, and `--timestamp` so the signature outlives
the certificate.

### Notarising

Signing is not enough on its own — Gatekeeper wants Apple's stamp too. Store the
credentials once:

```bash
xcrun notarytool store-credentials fuzzyaudio \
    --issuer "<ISSUER_ID>" --key-id "<KEY_ID>" --key AuthKey_<KEY_ID>.p8
```

then, after each build:

```bash
xcrun notarytool submit build/installer/Click-1.0.0-macOS.pkg \
    --keychain-profile fuzzyaudio --wait
xcrun stapler staple build/installer/Click-1.0.0-macOS.pkg
```

`--wait` blocks until Apple answers, usually minutes. On rejection,
`xcrun notarytool log <submission-id> --keychain-profile fuzzyaudio` says why;
the usual causes are a missing hardened runtime or an unsigned nested binary.

Stapling attaches the result to the package so it installs on a machine with no
internet connection. Notarising the `.pkg` covers everything inside it — the
bundles do not need separate submissions, as long as they were signed first.

### Checking it worked

```bash
codesign --verify --strict --verbose=2 build/Click_artefacts/Release/VST3/Click.vst3
pkgutil --check-signature build/installer/Click-1.0.0-macOS.pkg
spctl --assess --type install -vv build/installer/Click-1.0.0-macOS.pkg
xcrun stapler validate build/installer/Click-1.0.0-macOS.pkg
```

`spctl` should say *accepted* and *source=Notarized Developer ID*. Test on a
machine that has never seen the build — a signed package that fails on a clean
Mac is the failure that reaches customers.

Once this is in place, the quarantine workaround in the README is obsolete.

---

## Windows

### What to get

A code-signing certificate issued to **Fuzzy Audio LLC**, which means an
organisation validation check (registration documents, a verifiable phone
listing). Budget days, not minutes, for that.

Since the CA/Browser Forum tightened the rules in June 2023, the private key
must live in certified hardware. You no longer get a `.pfx` to copy around.
That leaves two shapes:

- **A hardware token** posted to you. Signing needs the token physically
  plugged in, which rules out unattended CI.
- **A cloud signing service** — the certificate lives in the CA's HSM and you
  authenticate over the network. This is what makes CI signing possible, and
  what most small vendors now use.

Prices, products and eligibility rules in this area change often, so confirm
current terms with the CA before buying; the only part that is stable is the
hardware-key requirement.

There are two grades. **EV** gets SmartScreen trust immediately. **OV** is
cheaper but starts with no reputation, so early downloads still get warned at
until enough installs accumulate. If the warning matters for launch, EV is the
one that avoids it.

### Signing

Sign in this order — the installer embeds the binaries, so signing it first
would leave unsigned files inside a signed wrapper:

```bat
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 ^
    build\Click_artefacts\Release\VST3\Click.vst3\Contents\x86_64-win\Click.vst3 ^
    build\Click_artefacts\Release\Standalone\Click.exe
```

(The VST3 "file" is a folder; the binary to sign is the DLL inside it.)

Then build the installer, letting Inno sign the result. `/S` names a sign tool
for the run and `/DSignTool` tells the script to use it:

```bat
iscc /Sfuzzy="signtool.exe sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $f" ^
     /DSignTool=fuzzy packaging\windows\Click.iss
```

Inno substitutes the file being signed for `$f`. Without `/DSignTool` the script
builds unsigned, which is what CI does today.

Always timestamp. Without `/tr` the signature dies with the certificate, and
every copy already sold stops validating.

### Checking it worked

```bat
signtool verify /pa /v build\installer\Click-1.0.0-Windows.exe
```

Then run the installer on a clean Windows machine and watch what SmartScreen
does. With EV, no warning. With OV, expect "Windows protected your PC" until
reputation builds — the publisher name shown should still read Fuzzy Audio LLC
rather than "Unknown publisher".

---

## In CI

The workflow builds both installers unsigned. To sign there, add the secrets and
set the environment before the packaging step.

macOS needs the certificates imported into a temporary keychain:

```yaml
- name: Import signing certificates
  if: runner.os == 'macOS'
  env:
    CERT_P12: ${{ secrets.MACOS_CERTIFICATES_P12 }}      # base64 of a .p12 holding both
    CERT_PASSWORD: ${{ secrets.MACOS_CERT_PASSWORD }}
  run: |
    echo "$CERT_P12" | base64 --decode > /tmp/certs.p12
    security create-keychain -p actions build.keychain
    security default-keychain -s build.keychain
    security unlock-keychain -p actions build.keychain
    security import /tmp/certs.p12 -k build.keychain -P "$CERT_PASSWORD" \
        -T /usr/bin/codesign -T /usr/bin/productsign
    security set-key-partition-list -S apple-tool:,apple: -s -k actions build.keychain
    rm /tmp/certs.p12
```

then run the existing step with `CLICK_CODESIGN_IDENTITY` and
`CLICK_INSTALLER_IDENTITY` set from secrets, and add the notarytool call after
it using an API key secret.

Windows CI signing only works with a cloud signing service — a hardware token
cannot be plugged into a GitHub runner. The provider supplies a `signtool`
replacement or a dlib; pass it as the `/S` command above.

Secrets to create, whichever provider you pick:

| Secret | Used by |
| --- | --- |
| `MACOS_CERTIFICATES_P12`, `MACOS_CERT_PASSWORD` | codesign, productsign |
| `MACOS_NOTARY_ISSUER_ID`, `MACOS_NOTARY_KEY_ID`, `MACOS_NOTARY_KEY_P8` | notarytool |
| provider-specific credentials | signtool on Windows |

Never commit any of these, and never let a signing credential reach a build log.
A leaked signing key is worse than a leaked licence key: it signs software as
you, and revoking it invalidates everything you have shipped.
