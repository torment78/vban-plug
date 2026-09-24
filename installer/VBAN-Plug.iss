#ifndef AppVersion
  #error AppVersion must be supplied by Build-Release.ps1
#endif
#ifndef PayloadDir
  #error PayloadDir must be supplied by Build-Release.ps1
#endif
#ifndef OutputPath
  #error OutputPath must be supplied by Build-Release.ps1
#endif
#if Ver < EncodeVer(6, 6, 0)
  #error Inno Setup 6.6 or later is required for the dark installer
#endif
#define DonateURL "https://ko-fi.com/msffixit"

[Setup]
AppId={{826C169A-BC2B-48D7-9442-C877455C006F}
AppName=VBAN Plug
AppVersion={#AppVersion}
AppVerName=VBAN Plug {#AppVersion}
AppPublisher=ElkaSoft
AppSupportURL={#DonateURL}
VersionInfoCompany=ElkaSoft
VersionInfoDescription=VBAN Plug TX + RX Installer
DefaultDirName={autopf}\ElkaSoft\VBAN Plug
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
UsePreviousAppDir=yes
UsePreviousPrivileges=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
; Current-user mode is for isolated packaging verification.
PrivilegesRequiredOverridesAllowed=commandline
WizardStyle=modern dark polar includetitlebar
WizardSizePercent=120,120
WizardImageFile=Assets\Wizard.png
WizardSmallImageFile=Assets\VBAN-Plug.png
SetupIconFile=Assets\VBAN-Plug.ico
UninstallDisplayIcon={app}\VBAN-Plug.ico
UninstallDisplayName=VBAN Plug TX + RX
OutputDir={#OutputPath}
OutputBaseFilename=VBAN-Plug-{#AppVersion}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
SetupLogging=yes
CloseApplications=yes
CloseApplicationsFilter=*.vst3
RestartApplications=no

[Files]
Source: "{#PayloadDir}\VBAN Plug TX.vst3\*"; DestDir: "{code:Vst3Directory}\VBAN Plug TX.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PayloadDir}\VBAN Plug RX.vst3\*"; DestDir: "{code:Vst3Directory}\VBAN Plug RX.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PayloadDir}\START HERE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\docs\RELEASING.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#PayloadDir}\docs\images\social-preview-tagged.jpg"; DestDir: "{app}\docs\images"; Flags: ignoreversion
Source: "{#PayloadDir}\Third-party notices\*"; DestDir: "{app}\Third-party notices"; Flags: ignoreversion recursesubdirs
Source: "Assets\VBAN-Plug.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "Assets\ElkaSoft.png"; Flags: dontcopy

Source: "{#PayloadDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\NOTICE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\docs\INSTALL.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#PayloadDir}\docs\images\editor-*.png"; DestDir: "{app}\docs\images"; Flags: ignoreversion
[Run]
Filename: "{code:Vst3Directory}"; Description: "Open the installed VST3 folder"; Flags: shellexec postinstall unchecked skipifsilent runasoriginaluser

[Code]
var
  DonateButton: TNewButton;
  WelcomeLogo, FinishedLogo: TBitmapImage;

function Vst3Directory(Param: String): String;
begin
  if IsAdminInstallMode then
    Result := ExpandConstant('{commoncf64}\VST3')
  else
    Result := ExpandConstant('{app}\VST3');
end;

procedure DonateClick(Sender: TObject);
var ErrorCode: Integer;
begin
  if not ShellExecAsOriginalUser('open', '{#DonateURL}', '', '', SW_SHOWNORMAL, ewNoWait, ErrorCode) then
    MsgBox('Open {#DonateURL} in your browser to support ElkaSoft.', mbInformation, MB_OK);
end;

procedure AddLogo(var Logo: TBitmapImage; ParentPage: TNewNotebookPage; LeftEdge: Integer);
begin
  Logo := TBitmapImage.Create(WizardForm);
  Logo.Parent := ParentPage;
  Logo.SetBounds(LeftEdge, ParentPage.Height - ScaleY(120), ScaleX(108), ScaleY(108));
  Logo.Stretch := True;
  Logo.PngImage.LoadFromFile(ExpandConstant('{tmp}\ElkaSoft.png'));
end;

procedure InitializeWizard;
begin
  ExtractTemporaryFile('ElkaSoft.png');
  WizardForm.WelcomeLabel1.Caption := 'VBAN Plug';
  WizardForm.WelcomeLabel2.Caption :=
    'Send and receive audio across your network.' + #13#10#13#10 +
    'TX sends a VBAN stream while your host audio passes through unchanged.' + #13#10 +
    'RX brings one incoming stream into your audio host.' + #13#10#13#10 +
    'Version {#AppVersion}  /  Windows x64  /  VST3' + #13#10#13#10 +
    'Close your audio host before continuing.';
  WizardForm.WelcomeLabel2.Height := ScaleY(180);
  AddLogo(WelcomeLogo, WizardForm.WelcomePage, WizardForm.WelcomeLabel2.Left);
  AddLogo(FinishedLogo, WizardForm.FinishedPage, WizardForm.FinishedLabel.Left);
  DonateButton := TNewButton.Create(WizardForm);
  DonateButton.Parent := WizardForm;
  DonateButton.Caption := 'Donate';
  DonateButton.SetBounds(ScaleX(16), WizardForm.NextButton.Top, ScaleX(90), WizardForm.NextButton.Height);
  DonateButton.Anchors := [akLeft, akBottom];
  DonateButton.OnClick := @DonateClick;
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := 'PLUG-INS' + NewLine +
    Space + 'VBAN Plug TX - transmit' + NewLine +
    Space + 'VBAN Plug RX - receive' + NewLine + NewLine +
    'VST3 INSTALL LOCATION' + NewLine +
    Space + Vst3Directory('') + NewLine + NewLine +
    'INSTALLER OPENED FROM' + NewLine +
    Space + ExpandConstant('{srcexe}') + NewLine + NewLine +
    'GUIDE AND UNINSTALLER' + NewLine +
    Space + ExpandConstant('{app}');
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then begin
    WizardForm.FinishedHeadingLabel.Caption := 'Your plug-ins are ready';
    WizardForm.FinishedLabel.Caption :=
      'VBAN Plug TX and RX are installed in:' + #13#10#13#10 +
      Vst3Directory('') + #13#10#13#10 +
      'Open your audio host and rescan VST3 plug-ins.' + #13#10 +
      'Choose VBAN Plug TX to send, or VBAN Plug RX to receive.';
    WizardForm.FinishedLabel.Height := ScaleY(150);
    WizardForm.RunList.Top := WizardForm.FinishedLabel.Top + ScaleY(154);
    WizardForm.RunList.Height := ScaleY(30);
  end;
end;
