#pragma once
// 应用版本号(单一来源)—— 关于/升级对话框及构建均引用此宏。
// 规则:每次改动递增第三位(patch):1.0.1 -> 1.0.2 -> 1.0.3 ...
//       git 提交信息带上当前版本号(如 "[v1.0.1] ..."）。
#define APP_VERSION "1.0.172"
#define APP_UPDATE_URL "https://www.sxfpga.cn/version.json"
// License 授权云(server/license, 见 specs/license管理需求.md)
#define APP_LICENSE_API "https://www.sxfpga.cn/license/api"
#define APP_LICENSE_CLIENT_KEY "5193f860d4777d6504cbf3bcf8b5ab23"
