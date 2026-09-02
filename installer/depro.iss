; depro(地听AI数据处理)Inno Setup 安装脚本
; 编译: ISCC.exe D:\gpr_software\installer\depro.iss
; 发布新版本时:改下面 AppVersion + OutputBaseFilename,并确保 D:\gpr_test 是最新 Release 部署。

#define MyAppName "地听AI数据处理"
#define MyAppNameEn "depro"
#define MyAppVersion "1.0.172"
#define MyAppPublisher "地听"
#define MyAppExeName "depro.exe"

[Setup]
AppId={{8F3B2A1C-depro-GPR-DITING}}
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
OutputBaseFilename=depro_Setup_{#MyAppVersion}
SetupIconFile=diting_logo.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
; v1.0.172 .DT/.DX 数据文件图标关联(见 [Registry])
ChangesAssociations=yes

[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式(&D)"; GroupDescription: "附加选项:"

[Files]
; 整个 Release 部署目录(含 Qt DLL、插件、MinGW 运行时、AI 模型),排除测试用的 log 目录
Source: "D:\gpr_test\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion; Excludes: "log,log\*,dzx_diag.log"
; v1.0.172 DT/DX 数据文件图标
Source: "D:\gpr_software\resources\diting_file.ico"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
; v1.0.172 .DT/.DX 文件图标关联(全局, 卸载自动清理; 不注册打开方式——软件暂不支持直接打开DT)
Root: HKCR; Subkey: ".DT"; ValueType: string; ValueName: ""; ValueData: "Diting.GprData"; Flags: uninsdeletekeyifempty
Root: HKCR; Subkey: ".DX"; ValueType: string; ValueName: ""; ValueData: "Diting.GprData"; Flags: uninsdeletekeyifempty
Root: HKCR; Subkey: "Diting.GprData"; ValueType: string; ValueName: ""; ValueData: "地听探地雷达数据文件"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Diting.GprData\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\diting_file.ico,0"; Flags: uninsdeletekey

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "立即启动 {#MyAppName}"; Flags: nowait postinstall skipifsilent
