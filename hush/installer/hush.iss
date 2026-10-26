; Inno Setup script for the Hush Windows installer.
;
;   iscc /DVersion=x.y.z /DArtefactsDir=<repo>\build-hush\Hush_artefacts\Release hush\installer\hush.iss

#ifndef Version
  #pragma error "Version not set - pass /DVersion=x.y.z (from hush/VERSION)"
#endif
#ifndef ArtefactsDir
  #define ArtefactsDir "..\..\build-hush\Hush_artefacts\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\build-hush"
#endif

[Setup]
AppId={{2FC56469-63DC-47BD-A7E9-B635D3B70B74}
AppName=Hush
AppVersion={#Version}
AppPublisher=dJONt (personal build)
AppPublisherURL=https://github.com/jonsaldivar1/tone3000-plugin
VersionInfoVersion={#Version}
DefaultDirName={autopf64}\Hush
DefaultGroupName=Hush
OutputDir={#OutputDir}
OutputBaseFilename=Hush-v{#Version}-windows-x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=hush.ico
UninstallDisplayIcon={app}\hush.ico
DisableProgramGroupPage=yes
PrivilegesRequired=admin
UninstallDisplayName=Hush

[Types]
Name: "full";   Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Standalone application"; Types: full custom
Name: "vst3";       Description: "VST3 plug-in";           Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: standalone

[Files]
Source: "hush.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\manual\Hush-Guide.pdf"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#ArtefactsDir}\Standalone\Hush.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#ArtefactsDir}\VST3\Hush.vst3\*"; DestDir: "{commoncf64}\VST3\Hush.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Hush\Hush"; Filename: "{app}\Hush.exe"; Components: standalone
Name: "{autoprograms}\Hush\Hush Guide"; Filename: "{app}\docs\Hush-Guide.pdf"
Name: "{autoprograms}\Hush\Uninstall Hush"; Filename: "{uninstallexe}"; IconFilename: "{app}\hush.ico"
Name: "{autodesktop}\Hush"; Filename: "{app}\Hush.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\docs\Hush-Guide.pdf"; Description: "Open the guide"; Flags: shellexec postinstall skipifsilent nowait
Filename: "{app}\Hush.exe"; Description: "Launch Hush"; Components: standalone; Flags: nowait postinstall skipifsilent
