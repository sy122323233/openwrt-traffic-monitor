# OpenWrt 流量监控系统

基于 OpenWrt 平台的实时网络流量监控工具，使用 C 语言 + libpcap 开发底层数据捕获，通过 HTTP 接口提供 JSON 数据，前端使用 ECharts 实现可视化展示。

##  目录

- [功能特性](#功能特性)
- [目录结构](#目录结构)
- [运行环境](#运行环境)
- [快速开始](#快速开始)
- [API 接口](#api-接口)
- [前端页面](#前端页面)
- [常见问题](#常见问题)
- [实验要求完成情况](#实验要求完成情况)
- [作者](#作者)


## 功能特性

-  实时捕获网络数据包（基于 libpcap）
-  统计每个 IP 的接收/发送累计流量
-  计算流量峰值速率
-  计算过去 2s、10s、40s 平均速率
-  命令行实时输出统计结果
-  HTTP 接口返回 JSON 格式数据
-  Web 前端可视化
-  支持交叉编译部署到 OpenWrt


## 目录结构

traffic_monitor/
├── src/
│ └── traffic_stats_http.c # 主程序（捕获+统计+HTTP服务）
├── build/ # 编译输出目录
│ └── traffic_monitor # 可执行文件
├── index.html # 流量监控前端页面
├── index_final.html # 整合页面（流量监控 + 防火墙）
├── CMakeLists.txt # CMake 配置
├── toolchain.cmake # 交叉编译配置
└── README.md # 项目说明


## 运行环境

| 环境 | 说明 |
|------|------|
| 开发系统 | Ubuntu 22.04 LTS（或 WSL） |
| 交叉编译工具 | OpenWrt SDK 24.10.0（x86_64） |
| 运行平台 | OpenWrt 24.10.0（x86_64） |
| 依赖库 | libpcap（运行时） |

安装基础依赖：

```bash
sudo apt update
sudo apt install -y build-essential cmake zstd wget libpcap-dev
```
## 快速开始

### 1. 配置交叉编译环境
```bash
# 加载环境变量
source ~/env.sh

# 验证交叉编译器
x86_64-openwrt-linux-musl-gcc --version
```

### 2. 编译项目
```bash
cd ~/traffic_monitor
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake
make
```
编译成功后生成 traffic_monitor 可执行文件。

### 3. 上传到 OpenWrt
在 Ubuntu 中启动 HTTP 服务器
```bash
cd ~/traffic_monitor/build
python3 -m http.server 8000
```
在 OpenWrt 中下载：
```bash
wget http://<Ubuntu_IP>:8000/traffic_monitor -O /root/traffic_http
chmod +x /root/traffic_http
```
### 4. 在 OpenWrt 中运行
```bash
/root/traffic_http
```
程序启动后运行命令行统计打印

### 5.访问 Web 界面
将 index.html 放到 HTTP 服务器目录：
```bash
cp ../index.html ~/traffic_monitor/build/
```
浏览器访问http://<你的Ubuntu_IP>:8000/index.html
