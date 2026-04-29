; =============================================================================
; Stardust — Windows Inno Setup Installer Script
; Installs:
;   - Stardust.vst3 → C:\Program Files\Common Files\VST3\
;   - Stardust.exe  → C:\Program Files\Stardust\ (standalone)
; =============================================================================
;
; Prerequisites:
;   1. Install Inno Setup 6+ from https://jrsoftware.org/isinfo.php
;   2. Build the plugin on Windows first:
;        cmake -B build -G "Visual Studio 17 2022" -A x64
;        cmake --build build --config Release
;   3. Compile this script with Inno Setup Compiler (ISCC.exe)
;
; Usage:
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\Stardust-installer.iss
; =============================================================================

#define MyAppName "Stardust"
#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif
#define MyAppPublisher "Stardust"

[Setup]
AppId={{7A3B8C5D-E9F1-4A2B-8C6D-1E3F5A7B9C0D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=..\build
OutputBaseFilename=Stardust-Setup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
PrivilegesRequired=admin
SetupIconFile=
UninstallDisplayIcon={app}\Stardust.exe
WizardStyle=modern
LicenseFile=
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Full installation (VST3 + Standalone)"
Name: "vst3only"; Description: "VST3 plugin only"
Name: "standalone"; Description: "Standalone application only"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 Plugin"; Types: full vst3only custom
Name: "standalone"; Description: "Standalone Application"; Types: full standalone custom

[Files]
; VST3 plugin — install to the system VST3 directory
Source: "..\build\Stardust_artefacts\Release\VST3\Stardust.vst3\*"; \
    DestDir: "{commoncf}\VST3\Stardust.vst3"; \
    Components: vst3; \
    Flags: ignoreversion recursesubdirs createallsubdirs

; Standalone exe
Source: "..\build\Stardust_artefacts\Release\Standalone\Stardust.exe"; \
    DestDir: "{app}"; \
    Components: standalone; \
    Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\Stardust.exe"; Components: standalone
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\Stardust.exe"; \
    Components: standalone; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; \
    GroupDescription: "Additional shortcuts:"; Components: standalone

[Run]
Filename: "{app}\Stardust.exe"; \
    Description: "Launch Stardust"; \
    Flags: nowait postinstall skipifsilent; \
    Components: standalone

[Messages]
WelcomeLabel2=This will install Stardust v{#MyAppVersion} on your computer.%n%nStardust adds musical grit, air, heat, dust, and broken-sampler color with one fast macro workflow.%n%nChoose a Flavor like Dust, Glass, Rust, Heat, Broken, or Glow, then use Character to shape GRIT and EXCITER together.%n%nThe VST3 plugin will be installed to the standard VST3 directory for use in FL Studio, Ableton Live, and other DAWs.

[Code]
// Show a post-install message reminding users to scan for plugins
procedure CurStepChanged(CurStep: TSetupStep);
begin
    if CurStep = ssDone then
    begin
        if IsComponentSelected('vst3') then
        begin
            MsgBox('Stardust VST3 has been installed to:' + #13#10 +
                   ExpandConstant('{commoncf}\VST3\Stardust.vst3') + #13#10#13#10 +
                   'To use it in your DAW:' + #13#10 +
                   '  FL Studio: Options > Manage plugins > Find more plugins > Start scan' + #13#10 +
                   '  Ableton: Preferences > Plug-Ins > Rescan' + #13#10 +
                   '  Other DAWs: Rescan your VST3 plugin directory',
                   mbInformation, MB_OK);
        end;
    end;
end;
