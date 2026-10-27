#!/usr/bin/env bash
# Instalador .pkg de macOS para Punto Tune Pro (VST3 + AU + Standalone opcional)
#
# Uso:  installer/mac/build_pkg.sh [carpeta_artefactos]
#   carpeta_artefactos por defecto: build/PuntoTunePro_artefacts/Release
#
# Variables opcionales (para firmar y notarizar, así no sale el aviso de Gatekeeper):
#   DEVELOPER_ID_APP="Developer ID Application: Tu Nombre (TEAMID)"
#   DEVELOPER_ID_INSTALLER="Developer ID Installer: Tu Nombre (TEAMID)"
#   NOTARY_PROFILE="nombre-del-perfil-de-notarytool"
set -euo pipefail

VERSION="1.0.0"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ART="${1:-$ROOT/build/PuntoTunePro_artefacts/Release}"
OUT="$ROOT/dist"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT"

VST3="$ART/VST3/Punto Tune Pro.vst3"
AU="$ART/AU/Punto Tune Pro.component"
APP="$ART/Standalone/Punto Tune Pro.app"

for p in "$VST3" "$AU"; do
  [ -d "$p" ] || { echo "No encuentro: $p"; exit 1; }
done

# --- firma de los binarios ----------------------------------------------------
sign() {
  if [ -n "${DEVELOPER_ID_APP:-}" ]; then
    codesign --force --deep --timestamp --options runtime -s "$DEVELOPER_ID_APP" "$1"
  else
    # firma "ad-hoc": necesaria para que cargue en Apple Silicon
    codesign --force --deep -s - "$1"
  fi
}
sign "$VST3"; sign "$AU"; [ -d "$APP" ] && sign "$APP"

# --- payloads ----------------------------------------------------------------
mkdir -p "$WORK/vst3/Library/Audio/Plug-Ins/VST3" \
         "$WORK/au/Library/Audio/Plug-Ins/Components" \
         "$WORK/app/Applications"
cp -R "$VST3" "$WORK/vst3/Library/Audio/Plug-Ins/VST3/"
cp -R "$AU"   "$WORK/au/Library/Audio/Plug-Ins/Components/"
HAVE_APP=0
if [ -d "$APP" ]; then cp -R "$APP" "$WORK/app/Applications/"; HAVE_APP=1; fi

SIGN_ARGS=()
[ -n "${DEVELOPER_ID_INSTALLER:-}" ] && SIGN_ARGS=(--sign "$DEVELOPER_ID_INSTALLER")

pkgbuild --root "$WORK/vst3" --identifier com.punto.puntotunepro.vst3 --version "$VERSION" \
         --install-location / "$WORK/PuntoTunePro-VST3.pkg"
pkgbuild --root "$WORK/au"   --identifier com.punto.puntotunepro.au   --version "$VERSION" \
         --install-location / "$WORK/PuntoTunePro-AU.pkg"
if [ "$HAVE_APP" = 1 ]; then
  pkgbuild --root "$WORK/app" --identifier com.punto.puntotunepro.app --version "$VERSION" \
           --install-location / "$WORK/PuntoTunePro-App.pkg"
fi

# --- distribución con selección de componentes -------------------------------
APP_CHOICE=""; APP_REF=""
if [ "$HAVE_APP" = 1 ]; then
  APP_CHOICE='<line choice="app"/>'
  APP_REF='<choice id="app" visible="true" title="Aplicación independiente" start_selected="false"><pkg-ref id="com.punto.puntotunepro.app"/></choice>
  <pkg-ref id="com.punto.puntotunepro.app" version="'"$VERSION"'">PuntoTunePro-App.pkg</pkg-ref>'
fi

cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>Punto Tune Pro</title>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <choices-outline>
    <line choice="vst3"/>
    <line choice="au"/>
    $APP_CHOICE
  </choices-outline>
  <choice id="vst3" visible="true" title="VST3" description="Para Ableton, FL Studio, Reaper, Logic (vía VST3 wrapper), Cubase, etc." start_selected="true">
    <pkg-ref id="com.punto.puntotunepro.vst3"/>
  </choice>
  <choice id="au" visible="true" title="Audio Unit (AU)" description="Para Logic Pro y GarageBand" start_selected="true">
    <pkg-ref id="com.punto.puntotunepro.au"/>
  </choice>
  $APP_REF
  <pkg-ref id="com.punto.puntotunepro.vst3" version="$VERSION">PuntoTunePro-VST3.pkg</pkg-ref>
  <pkg-ref id="com.punto.puntotunepro.au"   version="$VERSION">PuntoTunePro-AU.pkg</pkg-ref>
</installer-gui-script>
EOF

PKG="$OUT/PuntoTunePro-$VERSION-macOS.pkg"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "${SIGN_ARGS[@]}" "$PKG"

if [ -n "${NOTARY_PROFILE:-}" ] && [ -n "${DEVELOPER_ID_INSTALLER:-}" ]; then
  xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$PKG"
fi

echo "Instalador creado: $PKG"
