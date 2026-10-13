; Inno Setup script for the Click installer. Needs Inno Setup 6.3 or newer,
; for the x64compatible architecture identifier.
;
;   iscc packaging\windows\Click.iss
;
; Build Click first; the defaults below point at the CMake artefact tree. Both
; can be overridden, which is how CI passes the version it built:
;
;   iscc /DAppVersion=1.0.1 /DArtefacts=C:\path\to\Release packaging\windows\Click.iss
;
; To sign the installer, name a sign tool and define SignTool (docs/SIGNING.md).
; Sign the plugin binaries themselves before running this - Inno signs the
; installer it produces, not the files inside it:
;
;   iscc /Sfuzzy="signtool.exe sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $f" ^
;        /DSignTool=fuzzy packaging\windows\Click.iss

#ifndef AppVersion
  ; Keep in step with project(Click VERSION ...) in CMakeLists.txt.
  #define AppVersion "1.0.0"
#endif

#ifndef Artefacts
  #define Artefacts "..\..\build\Click_artefacts\Release"
#endif

#define AppName "Click"
#define Publisher "Fuzzy Audio LLC"

[Setup]
; Never change AppId: Windows identifies the installed product by it.
AppId={{4ACB3520-B80F-4035-98E2-E7B7E9D224FC}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#Publisher}
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\Fuzzy Audio\Click
DefaultGroupName=Fuzzy Audio
LicenseFile=..\eula.txt
SetupIconFile=Click.ico
UninstallDisplayIcon={uninstallexe}
OutputDir=..\..\build\installer
OutputBaseFilename=Click-{#AppVersion}-Windows
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; The plugin is 64-bit only, and Common Files\VST3 needs an administrator.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
; Both install locations are chosen on their own wizard pages instead.
DisableDirPage=yes
DisableProgramGroupPage=yes

#ifdef SignTool
SignTool={#SignTool}
#endif

[Types]
Name: "full"; Description: "VST3 plugin and standalone application"
Name: "custom"; Description: "Choose what to install"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin"; Types: full custom
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Files]
; Click.vst3 is a bundle: a folder the host loads, not a single file.
Source: "{#Artefacts}\VST3\Click.vst3\*"; DestDir: "{code:GetVST3Dir}\Click.vst3"; \
    Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\Click.exe"; DestDir: "{app}"; \
    Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\Click"; Filename: "{app}\Click.exe"; Components: standalone
Name: "{group}\Uninstall Click"; Filename: "{uninstallexe}"

[Code]
var
  AppDirPage: TInputDirWizardPage;
  VST3DirPage: TInputDirWizardPage;

procedure InitializeWizard;
begin
  AppDirPage := CreateInputDirPage(wpSelectComponents,
    'Standalone Application Location',
    'Where should the standalone application be installed?',
    'Setup will install the Click application into the following folder.',
    False, '');
  AppDirPage.Add('');
  AppDirPage.Values[0] := ExpandConstant('{autopf}\Fuzzy Audio\Click');

  VST3DirPage := CreateInputDirPage(AppDirPage.ID,
    'VST3 Plugin Location',
    'Where should the VST3 plugin be installed?',
    'Setup will install Click.vst3 into the following folder. Hosts scan the' +
    ' standard folder shown below; change it only if yours is set up to look' +
    ' somewhere else.',
    False, '');
  VST3DirPage.Add('');
  VST3DirPage.Values[0] := ExpandConstant('{commoncf64}\VST3');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;

  if PageID = AppDirPage.ID then
    Result := not WizardIsComponentSelected('standalone');

  if PageID = VST3DirPage.ID then
    Result := not WizardIsComponentSelected('vst3');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;

  if CurPageID = wpSelectComponents then
  begin
    if not (WizardIsComponentSelected('vst3') or WizardIsComponentSelected('standalone')) then
    begin
      MsgBox('Select at least one component to install.', mbError, MB_OK);
      Result := False;
    end;
  end

  // The standalone folder is {app}, which the hidden directory page would
  // otherwise have set.
  else if CurPageID = AppDirPage.ID then
    WizardForm.DirEdit.Text := AppDirPage.Values[0];
end;

function GetVST3Dir(Param: String): String;
begin
  Result := VST3DirPage.Values[0];
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo,
  MemoTypeInfo, MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := MemoComponentsInfo + NewLine + NewLine + 'Destination locations:' + NewLine;

  if WizardIsComponentSelected('vst3') then
    Result := Result + Space + VST3DirPage.Values[0] + '\Click.vst3' + NewLine;

  if WizardIsComponentSelected('standalone') then
    Result := Result + Space + AppDirPage.Values[0] + '\Click.exe' + NewLine;
end;
