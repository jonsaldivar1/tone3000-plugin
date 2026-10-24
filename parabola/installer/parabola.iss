; Inno Setup script for the Parabola Bass Z Windows installer.
;
;   iscc /DVersion=x.y.z /DArtefactsDir=<repo>\build-parabola\ParabolaBassZ_artefacts\Release parabola\installer\parabola.iss

#ifndef Version
  #pragma error "Version not set - pass /DVersion=x.y.z (from parabola/VERSION)"
#endif
#ifndef ArtefactsDir
  #define ArtefactsDir "..\..\build-parabola\ParabolaBassZ_artefacts\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\build-parabola"
#endif

[Setup]
AppId={{6F259B97-E133-4278-AD4F-91CE041C7A3C}
AppName=Parabola Bass Z
AppVersion={#Version}
AppPublisher=Parabola Bass Z (personal build)
AppPublisherURL=https://github.com/jonsaldivar1/tone3000-plugin
VersionInfoVersion={#Version}
DefaultDirName={autopf64}\Parabola Bass Z
DefaultGroupName=Parabola Bass Z
OutputDir={#OutputDir}
OutputBaseFilename=Parabola-Bass-Z-v{#Version}-windows-x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=parabola.ico
UninstallDisplayIcon={app}\parabola.ico
DisableProgramGroupPage=yes
PrivilegesRequired=admin
UninstallDisplayName=Parabola Bass Z

[Types]
Name: "full";   Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Standalone application"; Types: full custom
Name: "vst3";       Description: "VST3 plug-in";           Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: standalone

[Dirs]
; Where the LOAD buttons look first. Captures and IRs don't ship with the
; plugin (their licenses), so this is where your downloads go.
Name: "{userdocs}\Parabola Bass Z\NAMs"; Flags: uninsneveruninstall
Name: "{userdocs}\Parabola Bass Z\Cabs"; Flags: uninsneveruninstall

[Files]
Source: "parabola.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\manual\Parabola-Bass-Z-Quick-Start.pdf"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "..\manual\Parabola-Bass-Z-Manual.pdf"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#ArtefactsDir}\Standalone\Parabola Bass Z.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#ArtefactsDir}\VST3\Parabola Bass Z.vst3\*"; DestDir: "{commoncf64}\VST3\Parabola Bass Z.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Parabola Bass Z\Parabola Bass Z"; Filename: "{app}\Parabola Bass Z.exe"; Components: standalone
Name: "{autoprograms}\Parabola Bass Z\Parabola Bass Z Quick Start"; Filename: "{app}\docs\Parabola-Bass-Z-Quick-Start.pdf"
Name: "{autoprograms}\Parabola Bass Z\Parabola Bass Z Manual"; Filename: "{app}\docs\Parabola-Bass-Z-Manual.pdf"
Name: "{autoprograms}\Parabola Bass Z\Captures and cabs folder"; Filename: "{userdocs}\Parabola Bass Z"
Name: "{autoprograms}\Parabola Bass Z\Uninstall Parabola Bass Z"; Filename: "{uninstallexe}"; IconFilename: "{app}\parabola.ico"
Name: "{autodesktop}\Parabola Bass Z"; Filename: "{app}\Parabola Bass Z.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\docs\Parabola-Bass-Z-Quick-Start.pdf"; Description: "Open the Quick Start guide"; Flags: shellexec postinstall skipifsilent nowait
Filename: "{app}\Parabola Bass Z.exe"; Description: "Launch Parabola Bass Z"; Components: standalone; Flags: nowait postinstall skipifsilent
