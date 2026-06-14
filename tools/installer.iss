; Inno Setup script — compile with tools\build_installer.ps1
; Requires Inno Setup 6: https://jrsoftware.org/isdl.php

#define MyAppName "Top Down Survive"
#define MyAppVersion "1.0"
#define MyAppPublisher "Amos"
#define MyAppExeName "TopDownSurvive.exe"

[Setup]
AppId={{A7B3C9E1-4F2D-4A8B-9C1E-TopDownSurvive01}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\installer
OutputBaseFilename=TopDownSurvive_Setup
SetupIconFile=..\game.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\dist\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\characters\*"; DestDir: "{app}\characters"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\map_tileset\*"; DestDir: "{app}\map_tileset"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\nature_tileset\*"; DestDir: "{app}\nature_tileset"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\pickups\*"; DestDir: "{app}\pickups"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\sfx\*"; DestDir: "{app}\sfx"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\vfx\*"; DestDir: "{app}\vfx"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
