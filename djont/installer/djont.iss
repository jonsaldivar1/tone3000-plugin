; Inno Setup script for the dJONt Windows installer.
;
;   iscc /DVersion=x.y.z /DArtefactsDir=<repo>\build-djont\dJONt_artefacts\Release djont\installer\djont.iss

#ifndef Version
  #pragma error "Version not set - pass /DVersion=x.y.z (from djont/VERSION)"
#endif
#ifndef ArtefactsDir
  #define ArtefactsDir "..\..\build-djont\dJONt_artefacts\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\build-djont"
#endif

[Setup]
AppId={{B2CCCE22-3305-42C1-A333-990B2276A7CA}
AppName=dJONt
AppVersion={#Version}
AppPublisher=dJONt (personal build)
AppPublisherURL=https://github.com/jonsaldivar1/tone3000-plugin
VersionInfoVersion={#Version}
DefaultDirName={autopf64}\dJONt
DefaultGroupName=dJONt
OutputDir={#OutputDir}
OutputBaseFilename=dJONt-v{#Version}-windows-x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\dJONt.exe
DisableProgramGroupPage=yes
PrivilegesRequired=admin
UninstallDisplayName=dJONt

[Types]
Name: "full";   Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Standalone application"; Types: full custom
Name: "vst3";       Description: "VST3 plug-in";           Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: standalone

[Files]
Source: "{#ArtefactsDir}\Standalone\dJONt.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#ArtefactsDir}\VST3\dJONt.vst3\*"; DestDir: "{commoncf64}\VST3\dJONt.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\dJONt"; Filename: "{app}\dJONt.exe"; Components: standalone
Name: "{autodesktop}\dJONt"; Filename: "{app}\dJONt.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\dJONt.exe"; Description: "Launch dJONt"; Components: standalone; Flags: nowait postinstall skipifsilent
