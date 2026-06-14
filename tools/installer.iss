; Inno Setup script — compile with tools\build_installer.ps1
; Requires Inno Setup 6: https://jrsoftware.org/isdl.php

#define MyAppName "Top Down Survive Online"
#define MyAppFolder "top-down-survive-online"
#define MyAppVersion "1.0"
#define MyAppPublisher "Amos"
#define MyAppExeName "TopDownSurviveOnline.exe"
#define ProductionServerHost "thomas.proxy.rlwy.net"
#define ProductionServerPort "13034"
#define ProductionLaunchArgs "--transport tcp --host thomas.proxy.rlwy.net --port 13034"

[Setup]
AppId={{B8C4E2F1-5D3A-4B9C-A1E2-TopDownSurviveOnline}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppFolder}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\installer
OutputBaseFilename=top-down-survive-online_Setup
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
Source: "..\tools\play_online.bat"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Parameters: "{#ProductionLaunchArgs}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Parameters: "{#ProductionLaunchArgs}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Parameters: "{#ProductionLaunchArgs}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
