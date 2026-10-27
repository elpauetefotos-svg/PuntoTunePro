# Punto Tune Pro

Autotune en tiempo real (VST3 / AU / Standalone) con pestañas de Compresor, Delay y Reverb.

## Compilar
Requisitos: CMake 3.22+, compilador C++17 (Visual Studio 2022 en Windows, Xcode en macOS). JUCE se descarga solo.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --parallel

## Instaladores
- **Windows**: instala Inno Setup 6 y ejecuta
  `iscc /DBuildDir="RUTA\build\PuntoTunePro_artefacts\Release" installer\windows\PuntoTunePro.iss` -> `dist/PuntoTunePro-1.0.0-Windows-Setup.exe`
- **macOS**: `installer/mac/build_pkg.sh` -> `dist/PuntoTunePro-1.0.0-macOS.pkg` (VST3 + AU + app). Para firmar/notarizar define `DEVELOPER_ID_APP`, `DEVELOPER_ID_INSTALLER` y `NOTARY_PROFILE`.
- **Automático**: sube el repo a GitHub; `.github/workflows/build.yml` compila ambos en la nube y deja los instaladores como artefactos (o en Releases con un tag `v1.0.0`).

## Tests
    g++ -O2 -std=c++17 -ISource/dsp tests/dsp_test.cpp -o dsp_test && ./dsp_test
