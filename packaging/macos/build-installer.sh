#!/bin/bash
#
# Build the macOS installer from the bundles CMake produced.
#
#   packaging/macos/build-installer.sh [artefact-dir] [output-dir]
#
# Each format becomes its own component package so the installer can offer
# them separately. Their install locations are relative, which is what lets
# the Destination Select pane put them either under / or under the home
# folder; an absolute location would rule out the home install.
#
# Set these to sign; leave them unset for an unsigned build (docs/SIGNING.md):
#   CLICK_CODESIGN_IDENTITY    "Developer ID Application: ..."
#   CLICK_INSTALLER_IDENTITY   "Developer ID Installer: ..."

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)

artefacts=${1:-$root/build/Click_artefacts/Release}
output=${2:-$root/build/installer}

version=$(sed -n 's/^project(Click VERSION \([0-9][0-9.]*\).*/\1/p' "$root/CMakeLists.txt")

if [ -z "$version" ]; then
    echo "could not read the version out of CMakeLists.txt" >&2
    exit 1
fi

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

mkdir -p "$output" "$stage/packages"

# name, bundle, install location relative to the chosen destination
component()
{
    local name=$1 bundle=$2 location=$3

    if [ ! -d "$bundle" ]; then
        echo "missing $bundle - build Click before packaging it" >&2
        exit 1
    fi

    mkdir -p "$stage/$name"
    cp -R "$bundle" "$stage/$name/"

    # Sign the staged copy, so the build tree is left as cmake produced it.
    # The hardened runtime is what notarisation requires; without it the
    # notary service rejects the package.
    if [ -n "${CLICK_CODESIGN_IDENTITY:-}" ]; then
        codesign --force --timestamp --options runtime \
                 --sign "$CLICK_CODESIGN_IDENTITY" \
                 "$stage/$name/$(basename "$bundle")"
        codesign --verify --strict --verbose=1 "$stage/$name/$(basename "$bundle")"
    fi

    pkgbuild --quiet \
             --root "$stage/$name" \
             --identifier "com.fuzzyaudio.click.$name" \
             --version "$version" \
             --install-location "$location" \
             "$stage/packages/$name.pkg"
}

component vst3 "$artefacts/VST3/Click.vst3"      "Library/Audio/Plug-Ins/VST3"
component au   "$artefacts/AU/Click.component"   "Library/Audio/Plug-Ins/Components"
component app  "$artefacts/Standalone/Click.app" "Applications"

installer="$output/Click-$version-macOS.pkg"

# --resources is the folder the <license> path is resolved against.
productbuild --distribution "$here/distribution.xml" \
             --package-path "$stage/packages" \
             --resources "$root/packaging" \
             "$installer"

# A package is signed with a Developer ID *Installer* certificate, which is a
# different certificate from the Application one used above.
if [ -n "${CLICK_INSTALLER_IDENTITY:-}" ]; then
    productsign --sign "$CLICK_INSTALLER_IDENTITY" "$installer" "$installer.signed"
    mv "$installer.signed" "$installer"
    pkgutil --check-signature "$installer" | head -3
fi

echo "built $installer"

if [ -z "${CLICK_CODESIGN_IDENTITY:-}" ] || [ -z "${CLICK_INSTALLER_IDENTITY:-}" ]; then
    echo "note: unsigned - see docs/SIGNING.md" >&2
fi
