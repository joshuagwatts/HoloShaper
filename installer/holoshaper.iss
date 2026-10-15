; HoloShaper installer — Inno Setup
; Installs the VST3 bundle to the standard location, cleanly replacing old versions.

#define MyAppName "HoloShaper"
#define MyAppVersion "1.1.0"
#define MyAppPublisher "Holowatts"

[Setup]
AppId={{461a2746-7ea1-464d-a7c4-84d9d4e7e0b8}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
OutputDir=output
OutputBaseFilename=HoloShaper-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName={#MyAppName}
WizardStyle=modern

[Files]
Source: "..\build\HoloShaper_artefacts\Release\VST3\HoloShaper.vst3\*"; \
    DestDir: "{app}\HoloShaper.vst3"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}\Uninstall {#MyAppName}"; \
    Filename: "{uninstallexe}"
