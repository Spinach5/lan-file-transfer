# lanft — 局域网文件传输工具

[![Version](https://img.shields.io/badge/version-v1.0.0-blue.svg)](https://gitee.com/dzh258/lanft/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Gitee stars](https://gitee.com/dzh258/lanft/badge/star.svg?theme=dark)](https://gitee.com/dzh258/lanft)
[![GitHub stars](https://img.shields.io/github/stars/Spinach5/lanft?style=social)](https://github.com/Spinach5/lanft)

[English](./README.md) | 中文

快速、可靠的局域网文件传输工具，支持 GUI（SDL3）和 CLI 两种模式。支持 TCP/UDP、目录压缩传输、断点续传、设备扫描和传输历史。

---

## 特性

- **双模式**：GUI（SDL3 图形界面）和 CLI（命令行终端）
- **TCP & UDP**：可靠流式传输 或 自定义 ACK 确认重传
- **目录传输**：libarchive 自动压缩为 tar.gz，接收端自动解压
- **断点续传**：检测部分文件，从断点继续
- **局域网扫描**：多线程 TCP Connect 探测所有网卡子网
- **传输历史**：持久化记录（速度、进度、状态），保存到磁盘
- **nc 风格角色**：接收端 = 服务端（监听），发送端 = 客户端（连接）
- **跨平台**：Linux、Windows、macOS、Termux (Android)

---

## 构建选项

| CMake 选项 | 默认值 | 说明 |
|-----------|--------|------|
| `BUILD_GUI` | `ON` | 构建 SDL3 图形界面。设为 `OFF` 则仅 CLI（无 SDL3 依赖，体积更小） |

```bash
# 完整构建（GUI + CLI）
cmake .. -DBUILD_GUI=ON

# 仅 CLI（无 SDL3，适合服务器/嵌入式）
cmake .. -DBUILD_GUI=OFF
```

---

## 安装

### 前置条件

本项目使用 [Conan](https://conan.io/) 管理 C/C++ 依赖，请先安装 Conan：

```bash
# 安装 Conan（需要 Python 3.8+）如果因为权限问题无法安装,使用pipx安装
pip install conan # 或者 pipx install conan

# 创建默认配置文件
conan profile detect

```

> **Conan 官网**：[https://conan.io/](https://conan.io/) — 开源 C/C++ 包管理器。

你还需要 C 编译器和 CMake：

| 平台 | 安装命令 |
|----------|---------|
| Debian/Ubuntu | `sudo apt install -y build-essential cmake git` |
| Fedora/RHEL | `sudo dnf install -y gcc cmake git` |
| Arch | `sudo pacman -S --needed base-devel cmake git` |
| macOS | `brew install cmake git`（Xcode CLT 提供编译器）|
| Windows (MSYS2) | `pacman -S mingw-w64-ucrt-x86_64-{cmake,gcc,git}` |

### 构建（全平台 — Conan 管理依赖）

**第一步 — 安装依赖**（Conan 自动下载并构建所有库）：

```bash
git clone https://github.com/Spinach5/lanft.git
cd lanft
conan install . --build=missing --output-folder=build -s build_type=Release
```

此命令会自动拉取libwebsockets、libarchive、SDL3（以及 SDL3_ttf）。Conan 会自动检测你的平台；在 Termux/ARM 环境下会自动跳过 SDL3 和 SDL3_ttf。

**第二步 — 生成构建系统**（使用 Conan 生成的 CMake 预设）：

```bash
# GUI 模式（桌面 Linux / macOS / Windows）：
cmake --preset conan-release

# 仅 CLI 模式（服务器、Termux，或任何无 SDL3 的环境）：
cmake --preset conan-release -DBUILD_GUI=OFF
```

**第三步 — 编译：**

```bash
cmake --build build
# 可执行文件位于：build/lanft
```

### 构建（不使用 Conan / 系统包管理器）

如果你更习惯使用系统包管理器而非 Conan：

<details>
<summary>Linux (Debian/Ubuntu)</summary>

```bash
sudo apt install -y build-essential cmake git \
    libsdl3-dev libsdl3-ttf-dev libwebsockets-dev libarchive-dev

git clone https://github.com/Spinach5/lan-file-transfer.git lanft
cd lanft && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
# 仅 CLI：
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=OFF
make -j$(nproc)
```
</details>

<details>
<summary>Linux (Fedora/RHEL)</summary>

```bash
sudo dnf install -y gcc cmake git \
    SDL3-devel SDL3_ttf-devel libwebsockets-devel libarchive-devel
# 构建步骤同上
```
</details>

<details>
<summary>Linux (Arch)</summary>

```bash
sudo pacman -S --needed base-devel cmake git \
    sdl3 sdl3_ttf libwebsockets libarchive
# 构建步骤同上
```
</details>

<details>
<summary>macOS (Homebrew)</summary>

```bash
brew install cmake sdl3 sdl3_ttf libwebsockets libarchive
# 构建步骤同上
```
</details>

<details>
<summary>Termux (Android) — 仅 CLI 模式</summary>

Termux 不提供 SDL3 库，因此只支持 CLI 模式：

```bash
pkg update && pkg upgrade
pkg install cmake make clang git binutils python

# 安装 Conan
pip install conan

cd lanft

# 第一步：Conan 在 Termux 上自动跳过 SDL3/SDL3_ttf
conan install . --build=missing --output-folder=build -s build_type=Release

# 第二步：仅 CLI（无 SDL3）
cmake --preset conan-release -DBUILD_GUI=OFF

# 第三步：编译
cmake --build build

./build/lanft --help
```
</details>

### 安装到系统（可选）

```bash
sudo cp build/lanft /usr/local/bin/
```

---

## 项目结构

```
websocket/
├── CMakeLists.txt              # 构建系统
├── README.md                   # 英文文档
├── README_CN.md                # 中文文档
├── .gitignore
└── src/
    ├── main.c                  # SDL GUI 入口和事件循环
    ├── main_cli.c              # CLI-only 入口（BUILD_GUI=OFF 时使用）
    ├── cli.c                   # CLI 参数解析和终端传输
    ├── compat.h                # 跨平台兼容层（BSD/Winsock 抽象）
    ├── network.h / network.c   # libwebsockets 原始 TCP + BSD UDP
    ├── scanner.h / scanner.c   # 多线程局域网 TCP Connect 扫描
    ├── transfer.h / transfer.c # 文件收发、元数据握手、断点续传
    ├── protocol.h              # 共享常量和结构体
    └── ui.h / ui.c             # SDL3 GUI：标签页、按钮、进度条
```

### 按构建模式编译的文件

| 源文件 | BUILD_GUI=ON | BUILD_GUI=OFF |
|--------|:---:|:---:|
| `main.c` | ✅ | — |
| `main_cli.c` | — | ✅ |
| `ui.c` | ✅ | — |
| `scanner.c` | ✅ | — |
| `cli.c` | ✅ | ✅ |
| `network.c` | ✅ | ✅ |
| `transfer.c` | ✅ | ✅ |
| **二进制大小** | ~60 KB | ~40 KB |
| **链接 SDL3** | 是 | 否 |

---

## 依赖库

| 库 | 是否必需 | 版本 | 用途 |
|------|:---:|---------|------|
| SDL3 | 仅 GUI | ≥ 2.0 | 图形界面渲染和事件处理 |
| libwebsockets | 必需 | ≥ 4.0 | TCP 原始套接字管理 |
| libarchive | 必需 | ≥ 3.0 | 目录压缩/解压 (tar.gz) |
| pthreads | 必需 | 系统自带 | 多线程 |
| CMake | 必需 | ≥ 3.10 | 构建系统 |

---

## 使用方法

### CLI 模式（默认）

CLI 为默认模式 —— 不带 `--gui` 运行时默认进入命令行界面。

```bash
# 发送
lanft --mode=S --address=192.168.1.100 ./report.pdf
lanft -S -p 1234 ./video.mp4
lanft --protocol=UDP --mode=S --address=10.0.0.5 -p 5555 /home/user/docs/

# 接收
lanft --mode=R ./downloads/
lanft -R -p 5555 ./received/
lanft --mode=R --address=10.84.183.2 -p 5555 ./received/

# 使用自定义配置文件
lanft --profile=/path/to/config.toml --mode=R ./downloads/

# 信息
lanft --help
lanft --version
lanft --history
```

### GUI 模式

```bash
./lanft --gui
```

打开 SDL3 窗口，包含四个标签页：

| 标签页 | 功能 |
|--------|------|
| **Scan Devices** | 扫描局域网其他 lanft 实例。可配置端口。点击设备自动填入 IP。 |
| **Send File** | 选择文件/目录、Dir/File 切换、输入**接收端 IP**、Start Send。 |
| **Receive File** | 选择保存路径、**Listen IP** 默认 `0.0.0.0`。Listen & Receive。 |
| **History** | 传输历史表格：名称、时间、耗时、类型、端口、状态、进度、速度。 |

**典型流程：**
1. 接收端：Receive File → 选择保存目录 → Listen & Receive
2. 发送端：Send File → 选择文件 → 输入接收端 IP → Start Send
3. 两端显示进度条；完成后记录到 History

### 参数参考

| 短选项 | 长选项 | 默认值 | 说明 |
|-------|------|--------|------|
| | `--gui` | — | 启动 SDL3 图形界面（CLI 为默认） |
| `-h` | `--help` | — | 打印帮助并退出 |
| `-v` | `--version` | — | 打印版本号并退出 |
| `-S` | — | — | `--mode=S` 的缩写（发送） |
| `-R` | — | — | `--mode=R` 的缩写（接收） |
| | `--mode=S\|R` | *(必选)* | 传输模式 |
| | `--protocol=TCP\|UDP` | TCP | 传输协议 |
| `-p` | `--port=NUM` | 9876 | 端口号 |
| | `--address=IP` | `0.0.0.0` | 发送：接收端 IP。接收：监听 IP |
| | `--profile=PATH` | — | 指定配置文件（`--config` 的别名） |
| | `--history` | — | 打印历史记录 |

---

## 工作原理

### 协议

1. **元数据握手**：magic `FT01`、协议类型、文件名、总大小、标志位
2. **元数据响应**：magic + 续传偏移量
3. **数据传输**：TCP 原始字节流 / UDP 分块 ACK 确认
4. 目录模式：数据为 `.tar.gz` 压缩包，接收端自动解压

### 角色模型（nc 风格）

```
接收端（服务端）：net_listen → net_accept → 接收元数据 → 接收数据
发送端（客户端）：net_connect → 发送元数据 → 读取响应 → 发送数据
```

### 退出码

| 退出码 | 含义 |
|--------|------|
| 0 | 传输成功完成 |
| 1 | 错误（参数非法、连接失败、传输中断） |

---

## 许可证

MIT

## 星折线图
[![Star History Chart](https://api.star-history.com/chart?repos=Spinach5/lanft&type=date&logscale&legend=top-left)](https://www.star-history.com/?repos=Spinach5%2Flanft&type=date&logscale=&legend=top-left)
