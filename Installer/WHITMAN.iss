; WHITMAN installer (Inno Setup 6). Built by .github/workflows/build-windows.yml:
;   iscc /DAppVersion=1.0.12 Installer\WHITMAN.iss
; Installs for the current user (no admin prompt), so WHITMAN can update itself.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{C4A81E5D-3B27-4F96-8E0A-7D52B9F1A3C8}
AppName=WHITMAN
AppVersion={#AppVersion}
AppVerName=WHITMAN {#AppVersion}
AppPublisher=PEQUOD
DefaultDirName={autopf}\WHITMAN
DefaultGroupName=WHITMAN
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayName=WHITMAN
UninstallDisplayIcon={app}\WHITMAN.exe
OutputDir=..\dist
OutputBaseFilename=WHITMAN-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=WHITMAN.ico
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: "desktopicon"; Description: "Put a WHITMAN shortcut on the desktop"; GroupDescription: "Shortcuts:"

[Files]
Source: "..\build\Whitman_artefacts\Release\WHITMAN.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Resources\Fonts\*.txt"; DestDir: "{app}\Licences"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\WHITMAN"; Filename: "{app}\WHITMAN.exe"
Name: "{autodesktop}\WHITMAN"; Filename: "{app}\WHITMAN.exe"; Tasks: desktopicon

[Run]
; normal install: offer to start it; update from inside WHITMAN (silent): start it again by itself
Filename: "{app}\WHITMAN.exe"; Description: "Start WHITMAN"; Flags: nowait postinstall skipifsilent
Filename: "{app}\WHITMAN.exe"; Flags: nowait; Check: WizardSilent
