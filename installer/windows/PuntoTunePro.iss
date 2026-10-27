; Instalador de Windows para Punto Tune Pro (Inno Setup 6)
; Compilar:  iscc /DBuildDir="C:\ruta\build\PuntoTunePro_artefacts\Release" PuntoTunePro.iss

#ifndef BuildDir
  #define BuildDir "..\..\build\PuntoTunePro_artefacts\Release"
#endif
#define AppName    "Punto Tune Pro"
#define AppVersion "1.0.0"

[Setup]
AppId={{8F1C2B6E-4C77-4F3B-9A71-5C6D3A0E9B11}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Punto
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir=..\..\dist
OutputBaseFilename=PuntoTunePro-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName={#AppName}

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";   Description: "Instalación completa"
Name: "custom"; Description: "Personalizada"; Flags: iscustom

[Components]
Name: "vst3";       Description: "Plugin VST3 (para tu DAW)"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Aplicación independiente";  Types: full

[Files]
; VST3 -> carpeta estándar de plugins VST3
Source: "{#BuildDir}\VST3\Punto Tune Pro.vst3\*"; DestDir: "{commoncf64}\VST3\Punto Tune Pro.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
; Standalone
Source: "{#BuildDir}\Standalone\Punto Tune Pro.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\Punto Tune Pro"; Filename: "{app}\Punto Tune Pro.exe"; Components: standalone
Name: "{group}\Desinstalar Punto Tune Pro"; Filename: "{uninstallexe}"

[Messages]
spanish.FinishedLabel=Punto Tune Pro se ha instalado. Abre tu DAW y vuelve a escanear los plugins VST3 si no aparece.
