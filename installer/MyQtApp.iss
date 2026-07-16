; MyQtApp(劳雷AI数据处理)Inno Setup 安装脚本
; 编译: ISCC.exe D:\gpr_software\installer\MyQtApp.iss
; 发布新版本时:改下面 AppVersion + OutputBaseFilename,并确保 D:\gpr_test 是最新 Release 部署。

#define MyAppName "劳雷AI数据处理"
#define MyAppNameEn "MyQtApp"
#define MyAppVersion "1.0.26"
#define MyAppPublisher "劳雷"
#define MyAppExeName "MyQtApp.exe"

[Setup]
AppId={{8F3B2A1C-MyQtApp-GPR-LAUREL}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
OutputDir=D:\gpr_release
OutputBaseFilename=MyQtApp_Setup_{#MyAppVersion}
SetupIconFile=laurel_logo.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin

[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式(&D)"; GroupDescription: "附加选项:"

[Files]
; 整个 Release 部署目录(含 Qt DLL、插件、MinGW 运行时、AI 模型),排除测试用的 log 目录
Source: "D:\gpr_test\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion; Excludes: "log,log\*"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "立即启动 {#MyAppName}"; Flags: nowait postinstall skipifsilent
