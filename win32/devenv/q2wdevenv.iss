; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{011CCE1C-8593-4066-9EB4-F02669E3F651}
AppName=Quake2World Development Environment
AppVersion=3.3.0
;AppVerName=Quake2World Development Environment 3.3.0
AppPublisher=Marcel Wysocki
AppPublisherURL=http://www.quake2world.net/
AppSupportURL=http://www.quake2world.net/
AppUpdatesURL=http://www.quake2world.net/
DefaultDirName=c:/q2wdevenv
DefaultGroupName=Quake2World Development Environment
AllowNoIcons=yes
LicenseFile=C:\q2wdevenv\LICENSE.txt
OutputBaseFilename=q2wdevenv-3.3.0-1
Compression=lzma
SolidCompression=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 0,6.1

[Files]
Source: "C:\q2wdevenv\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\Quake2World Development Environment"; Filename: "{app}\msys\msys.bat"
Name: "{group}\{cm:ProgramOnTheWeb,Quake2World Development Environment}"; Filename: "http://www.quake2world.net/"
Name: "{group}\{cm:UninstallProgram,Quake2World Development Environment}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\Quake2World Development Environment"; Filename: "{app}\msys\msys.bat"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\Quake2World Development Environment"; Filename: "{app}\msys\msys.bat"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\msys\msys.bat"; Description: "{cm:LaunchProgram,Quake2World Development Environment}"; Flags: shellexec postinstall skipifsilent

