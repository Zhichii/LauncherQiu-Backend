# LauncherQiu

LauncherQiu 启动器后端部分

## 项目简介

LauncherQiu 是一个 Minecraft 游戏启动器后端项目
使用 C++20 开发，提供启动器核心的游戏启动、账号认证、资源管理等功能

## 技术栈

- **语言**: C++20
- **构建工具**: Python + G++ 或 Visual Studio C++
- **依赖库**:
  - [jsoncpp](https://github.com/open-source-parsers/jsoncpp) - JSON 解析
  - [minizip](https://github.com/nmoinakze/minizip) - ZIP 文件处理
  - [OpenSSL](https://www.openssl.org/) - SSL/TLS 支持
  - [zlib](https://www.zlib.net/) - 数据压缩

## 核心功能

### 账号系统 (`account/`)
- Microsoft 账号 OAuth2 认证
- Xbox Live 认证
- Minecraft 官方账号验证
- 本地 HTTP 回调服务器处理登录流程

Microsoft 登录不会在源码中内置 `client_id`。
运行登录流程前，需要由调用方传入 `OAuthConfig.client_id`，或设置环境变量
`LAUNCHERQIU_AZURE_CLIENT_ID`（兼容 `AZURE_CLIENT_ID`）。

### 实例管理 (`instance.cpp`)
- 解析 Minecraft 版本清单 (version.json)
- 处理库文件依赖
- 原生库提取 (natives)
- 跨平台支持 (Windows/Linux/macOS)

### 工具库
- `strings.hpp` - 字符串处理工具 (分割、替换、切片等)
- `config.hpp` - 跨平台配置宏

## 项目结构

```
launcher/
├── account/           # 账号认证模块
│   └── account.cpp
├── libs/              # 第三方库编译文件
├── Build/             # 编译输出目录
├── config.hpp         # 平台配置
├── instance.hpp       # 实例管理头文件
├── instance.cpp       # 实例管理实现
├── strings.hpp        # 字符串工具
├── build.py           # 构建脚本
└── test.cpp           # 测试入口
```

## 构建指南

### 环境要求

- Python 3
- 二选一：
  - G++ (支持 C++20)
  - Visual Studio / Build Tools，包含 MSVC C++ 工具链
- 依赖库：
  - `jsoncpp`
  - `minizip`
  - `zlib`
  - 如果使用 G++ 路线，仍需要 `OpenSSL`
  - 如果使用 Visual Studio 路线，推荐通过 `vcpkg` 安装依赖

### 编译步骤

```bash
# 编译所有模块
python build.py all

# 编译指定模块
python build.py test

# 编译测试入口并附带账号模块
python build.py test account/account
```

脚本会自动选择可用工具链：

- 优先使用 `g++`
- Windows 下如果没有 `g++`，会自动查找 Visual Studio 并使用 `cl`

编译产物位于 `Build/` 目录，Windows 下生成 `LauncherQiu-Core.exe`。

## 后端对跨平台的理论支持

| 平台 | 状态 |
|------|------|
| Windows | ✅ 支持 |
| Linux | ✅ 支持 |
| macOS | ✅ 支持 |
| iOS | ❌ 不支持 |

## 许可证

本项目包含以下第三方许可证：
- [LICENSE](LICENSE) - 主项目许可证
- [LICENSE_JSONCPP](LICENSE_JSONCPP) - jsoncpp 许可证

## 联系方式

如需技术支持或功能请求，请联系：Qiuyixuan_last@outlook.com

## 注意事项

- 本项目为后端核心，需配合前端 UI 使用
- 开发中
