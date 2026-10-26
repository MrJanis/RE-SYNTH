; RE-SYNTH: 64-bit system-wide VST3 installer for developer testing.
; Requires the plugin bundle from the Windows JUCE build and ISCC preprocessor defines.
#ifndef PluginSource
  #error PluginSource must reference the compiled RE-SYNTH.vst3 directory
#endif
#ifndef OutDir
  #error OutDir must reference an output directory
#endif
#ifndef ReleaseVersion
  #define ReleaseVersion "0.5.1"
#endif

[Setup]
AppId={{4F09BD5D-78E4-49F6-B86D-17731DA3A816}
AppName=RE-SYNTH
AppVersion={#ReleaseVersion}
AppPublisher=RE-SYNTH
AppVerName=RE-SYNTH {#ReleaseVersion}
DefaultDirName={autopf}\RE-SYNTH
DisableDirPage=yes
DisableProgramGroupPage=yes
CreateAppDir=no
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
OutputDir={#OutDir}
OutputBaseFilename=RE-SYNTH-Setup-Windows
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=RE-SYNTH VST3
WizardStyle=modern
; Not commercially code signed. Development/test installation only.

[Files]
; JUCE VST3 bundles are directories containing Contents/x86_64-win/*.vst3.
Source: "{#PluginSource}\*"; DestDir: "{commoncf}\VST3\RE-SYNTH.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[Messages]
FinishedHeadingLabel=RE-SYNTH installation complete
FinishedLabel=RE-SYNTH VST3 has been installed. Restart your DAW and rescan plugins if necessary.
