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
SetupIconFile=djont.ico
UninstallDisplayIcon={app}\djont.ico
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

[InstallDelete]
; Up to 0.5 the Start menu had a single dJONt shortcut; it's a folder now.
Type: files; Name: "{autoprograms}\dJONt.lnk"

[Files]
Source: "djont.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\manual\dJONt-Quick-Start.pdf"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "..\manual\dJONt-Manual.pdf"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#ArtefactsDir}\Standalone\dJONt.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#ArtefactsDir}\VST3\dJONt.vst3\*"; DestDir: "{commoncf64}\VST3\dJONt.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\dJONt\dJONt"; Filename: "{app}\dJONt.exe"; Components: standalone
Name: "{autoprograms}\dJONt\dJONt Quick Start"; Filename: "{app}\docs\dJONt-Quick-Start.pdf"
Name: "{autoprograms}\dJONt\dJONt Manual"; Filename: "{app}\docs\dJONt-Manual.pdf"
Name: "{autoprograms}\dJONt\Uninstall dJONt"; Filename: "{uninstallexe}"; IconFilename: "{app}\djont.ico"
Name: "{autodesktop}\dJONt"; Filename: "{app}\dJONt.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\docs\dJONt-Quick-Start.pdf"; Description: "Open the Quick Start guide"; Flags: shellexec postinstall skipifsilent nowait
Filename: "{app}\dJONt.exe"; Description: "Launch dJONt"; Components: standalone; Flags: nowait postinstall skipifsilent
