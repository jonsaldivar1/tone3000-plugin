; Inno Setup script for the T3KJ Windows installer (fork of TONE3000).
; T3KJ installs next to the official TONE3000: its own app name, folder,
; plug-in file names and uninstall entry. It reads and writes the same
; TONE3000 presets folder, so presets are shared between the two.
;
;   iscc /DVersion=x.y.z /DArtefactsDir=<repo>\build\t3kj\TONE3000_artefacts\Release script\installer\windows\t3kj.iss
;
; Defines (override with /D on the command line):
;   Version       plugin version string, REQUIRED (repo-root VERSION file)
;   ArtefactsDir  path to the JUCE Release artefacts dir
;   OutputDir     where the setup exe is written

#ifndef Version
  #pragma error "Version not set - pass /DVersion=x.y.z (from the repo-root VERSION file)"
#endif
#ifndef ArtefactsDir
  #define ArtefactsDir "..\..\..\build\t3kj\TONE3000_artefacts\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\..\build"
#endif

[Setup]
; Distinct AppId from TONE3000's, so installing or uninstalling T3KJ never
; touches the official install.
AppId={{32865E4C-9DDA-4F5E-AF7F-4D63A6352548}
AppName=T3KJ
AppVersion={#Version}
AppPublisher=T3KJ (personal fork of TONE3000)
; Renamed from TK3J (same AppId, so this upgrades it): install to the new
; folder, not the old one, and clear the old files below.
UsePreviousAppDir=no
UsePreviousGroup=no
AppPublisherURL=https://github.com/jonsaldivar1/tone3000-plugin
VersionInfoVersion={#Version}
DefaultDirName={autopf64}\T3KJ
DefaultGroupName=T3KJ
OutputDir={#OutputDir}
OutputBaseFilename=T3KJ-v{#Version}-windows-x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
InfoBeforeFile=..\..\..\LICENSE
WizardStyle=modern
DisableWelcomePage=no
WizardImageFile=wizard-image-100.bmp,wizard-image-200.bmp
WizardSmallImageFile=wizard-small-100.bmp,wizard-small-200.bmp
SetupIconFile=t3kj.ico
UninstallDisplayIcon={app}\T3KJ.exe
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=admin
UninstallDisplayName=T3KJ

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
Source: "{#ArtefactsDir}\Standalone\T3KJ.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#ArtefactsDir}\VST3\T3KJ.vst3\*"; DestDir: "{commoncf64}\VST3\T3KJ.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ArtefactsDir}\CLAP\T3KJ.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
; The factory folder belongs to TONE3000 (shared path). Add files only when
; missing and never remove them, so T3KJ can't clobber or delete the
; official install's presets.
Source: "..\..\..\resources\factory-presets\*.t3kpreset"; DestDir: "{commonappdata}\TONE3000\Presets\Factory"; Components: presets; Flags: onlyifdoesntexist uninsneveruninstall

; The build from before the rename installed as TK3J: remove its files so
; the DAW doesn't list two copies. (Same plugin IDs, so projects that used
; TK3J open with T3KJ.)
[InstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\TK3J.vst3"
Type: files; Name: "{commoncf64}\CLAP\TK3J.clap"
Type: filesandordirs; Name: "{autopf64}\TK3J"
Type: files; Name: "{autoprograms}\TK3J.lnk"
Type: files; Name: "{autodesktop}\TK3J.lnk"

[Icons]
Name: "{autoprograms}\T3KJ"; Filename: "{app}\T3KJ.exe"; Components: standalone
Name: "{autodesktop}\T3KJ"; Filename: "{app}\T3KJ.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\T3KJ.exe"; Description: "Launch T3KJ"; Components: standalone; Flags: nowait postinstall skipifsilent

[Code]
// The standalone app keeps its audio settings (device, ASIO driver,
// channels, buffer) in %APPDATA%\<name>.settings: carry TK3J's over the
// first time, so the rename doesn't reset them.
procedure CurStepChanged(CurStep: TSetupStep);
var
  OldFile, NewFile: String;
begin
  if CurStep = ssPostInstall then
  begin
    OldFile := ExpandConstant('{userappdata}\TK3J.settings');
    NewFile := ExpandConstant('{userappdata}\T3KJ.settings');
    if FileExists(OldFile) and not FileExists(NewFile) then
      FileCopy(OldFile, NewFile, True);
  end;
end;
