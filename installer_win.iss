; Inno Setup script for compiling the Kronos VST3 installer on Windows

[Setup]
AppName=Kronos
AppVersion=1.0.0
AppPublisher=Algebra Within
DefaultDirName={commoncf}\VST3\Algebra Within\Kronos.vst3
DisableDirPage=yes
UsePreviousAppDir=no
OutputBaseFilename=Kronos_Windows_Installer
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
DisableProgramGroupPage=yes
DisableWelcomePage=no

[Files]
; Copy VST3 bundle contents to the standard 64-bit Common Files VST3 folder
Source: "Builds\VisualStudio2022\x64\Release\VST3\Kronos.vst3\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion
