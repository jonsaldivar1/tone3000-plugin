; Inno Setup script for the TK3J Windows installer (fork of TONE3000).
; TK3J installs next to the official TONE3000: its own app name, folder,
; plug-in file names and uninstall entry. It reads and writes the same
; TONE3000 presets folder, so presets are shared between the two.
;
;   iscc /DVersion=x.y.z /DArtefactsDir=<repo>\build\plugin\TONE3000_artefacts\Release script\installer\windows\tk3j.iss
;
; Defines (override with /D on the command line):
;   Version       plugin version string, REQUIRED (repo-root VERSION file)
;   ArtefactsDir  path to the JUCE Release artefacts dir
;   OutputDir     where the setup exe is written

#ifndef Version
  #pragma error "Version not set - pass /DVersion=x.y.z (from the repo-root VERSION file)"
#endif
#ifndef ArtefactsDir
  #define ArtefactsDir "..\..\..\build\plugin\TONE3000_artefacts\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\..\build"
#endif

[Setup]
; Distinct AppId from TONE3000's, so installing or uninstalling TK3J never
; touches the official install.
AppId={{32865E4C-9DDA-4F5E-AF7F-4D63A6352548}
AppName=TK3J
AppVersion={#Version}
AppPublisher=TK3J (personal fork of TONE3000)
AppPublisherURL=https://github.com/jonsaldivar1/tone3000-plugin
VersionInfoVersion={#Version}
DefaultDirName={autopf64}\TK3J
DefaultGroupName=TK3J
OutputDir={#OutputDir}
OutputBaseFilename=TK3J-v{#Version}-windows-x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
InfoBeforeFile=..\..\..\LICENSE
WizardStyle=modern
DisableWelcomePage=no
WizardImageFile=wizard-image-100.bmp,wizard-image-200.bmp
WizardSmallImageFile=wizard-small-100.bmp,wizard-small-200.bmp
SetupIconFile=tk3j.ico
UninstallDisplayIcon={app}\TK3J.exe
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=admin
UninstallDisplayName=TK3J

[Types]
Name: "full";   Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Standalone application"; Types: full custom
Name: "vst3";       Description: "VST3 plug-in";           Types: full custom
Name: "clap";       Description: "CLAP plug-in";           Types: full custom
Name: "presets";    Description: "Factory presets (only if missing; shared with TONE3000)"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: standalone

[Files]
Source: "{#ArtefactsDir}\Standalone\TK3J.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#ArtefactsDir}\VST3\TK3J.vst3\*"; DestDir: "{commoncf64}\VST3\TK3J.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ArtefactsDir}\CLAP\TK3J.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
; The factory folder belongs to TONE3000 (shared path). Add files only when
; missing and never remove them, so TK3J can't clobber or delete the
; official install's presets.
Source: "..\..\..\resources\factory-presets\*.t3kpreset"; DestDir: "{commonappdata}\TONE3000\Presets\Factory"; Components: presets; Flags: onlyifdoesntexist uninsneveruninstall

[Icons]
Name: "{autoprograms}\TK3J"; Filename: "{app}\TK3J.exe"; Components: standalone
Name: "{autodesktop}\TK3J"; Filename: "{app}\TK3J.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\TK3J.exe"; Description: "Launch TK3J"; Components: standalone; Flags: nowait postinstall skipifsilent
