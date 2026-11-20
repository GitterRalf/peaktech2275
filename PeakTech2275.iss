; Build PeakTech2275.exe and deploy its Qt runtime with build.ps1 before compiling.
; Install the serial adapter driver separately if required.
#define ReadQuoted(Text) Copy(Text, Pos('"', Text) + 1, Pos('"', Copy(Text, Pos('"', Text) + 1, Len(Text))) - 1)
#define ProjectFile FileOpen(SourcePath + "CMakeLists.txt")
#expr FileRead(ProjectFile)
#define ProjectDeclaration FileRead(ProjectFile)
#define VersionTail Copy(ProjectDeclaration, Pos(" VERSION ", ProjectDeclaration) + Len(" VERSION "), Len(ProjectDeclaration))
#define PeakTech2275Version Copy(VersionTail, 1, Pos(" ", VersionTail) - 1)
#expr FileClose(ProjectFile)
#define IconResource FileOpen(SourcePath + "assets\app.rc")
#define PeakTech2275Icon ReadQuoted(FileRead(IconResource))
#expr FileClose(IconResource)
#define PeakTech2275Name "PeakTech 2275"
#define PeakTech2275Exe "PeakTech2275.exe"

[Setup]
AppId={{7CF0A848-F5AC-428A-804D-463BA718FA7D}
AppName={#PeakTech2275Name}
AppVersion={#PeakTech2275Version}
AppVerName={#PeakTech2275Name} {#PeakTech2275Version}
AppPublisher=Ralf Tralow
VersionInfoVersion={#PeakTech2275Version}
VersionInfoDescription={#PeakTech2275Name} Setup
DefaultDirName={pf}\{#PeakTech2275Name}
DefaultGroupName={#PeakTech2275Name}
AllowNoIcons=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; Inno 5.3.5 can see modern Windows as 6.2 due to its legacy manifest.
SetupIconFile=assets\{#PeakTech2275Icon}
UninstallDisplayIcon={app}\{#PeakTech2275Exe}
OutputDir=installer
OutputBaseFilename=PeakTech2275-Setup-{#PeakTech2275Version}-x64
Compression=lzma
SolidCompression=yes

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Files]
Source: "build\{#PeakTech2275Exe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\generic\*.dll"; DestDir: "{app}\generic"; Flags: ignoreversion
Source: "build\iconengines\*.dll"; DestDir: "{app}\iconengines"; Flags: ignoreversion
Source: "build\imageformats\*.dll"; DestDir: "{app}\imageformats"; Flags: ignoreversion
Source: "build\networkinformation\*.dll"; DestDir: "{app}\networkinformation"; Flags: ignoreversion
Source: "build\platforms\*.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "build\styles\*.dll"; DestDir: "{app}\styles"; Flags: ignoreversion
Source: "build\tls\*.dll"; DestDir: "{app}\tls"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "THIRD-PARTY-NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "licenses\*.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion

[Icons]
Name: "{group}\{#PeakTech2275Name}"; Filename: "{app}\{#PeakTech2275Exe}"; WorkingDir: "{app}"
Name: "{commondesktop}\{#PeakTech2275Name}"; Filename: "{app}\{#PeakTech2275Exe}"; WorkingDir: "{app}"; Tasks: desktopicon
