; Inno Setup 安装包定义。
;
;   ISCC.exe /DStage=<待打包目录> /DVersion=1.2.3 /DArch=x64 /O<输出目录> gomoku.iss
;
; PrivilegesRequired=lowest 配合 {autopf}，在普通用户下会落到
; %LOCALAPPDATA%\Programs\Gomoku，同样不需要管理员。

#ifndef Stage
  #define Stage "."
#endif
#ifndef Version
  #define Version "0.0.0"
#endif
#ifndef Arch
  #define Arch "x64"
#endif

[Setup]
AppName=五子棋
AppVersion={#Version}
AppVerName=五子棋 {#Version}
AppPublisher=iamlinxuhan
DefaultDirName={autopf}\Gomoku
DefaultGroupName=五子棋
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputBaseFilename=Gomoku-{#Version}-windows-{#Arch}-inno
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed={#Arch}
ArchitecturesInstallIn64BitMode={#Arch}
UninstallDisplayName=五子棋
UninstallDisplayIcon={app}\Gomoku.exe
AllowNoIcons=yes

[Languages]
; 向导界面还是英文：Inno 官方发行版不带简体中文的 .isl，引用不存在的文件会直接
; 编译失败。想要中文向导的话，把 ChineseSimplified.isl 放进 Inno 的 Languages
; 目录，再在这里加一行 Name: "cn"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"。
; 应用名、快捷方式和任务描述不受影响，一直是中文。
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务："

[Files]
Source: "{#Stage}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\五子棋"; Filename: "{app}\Gomoku.exe"
Name: "{group}\卸载五子棋"; Filename: "{uninstallexe}"
Name: "{autodesktop}\五子棋"; Filename: "{app}\Gomoku.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Gomoku.exe"; Description: "立即启动五子棋"; Flags: nowait postinstall skipifsilent
