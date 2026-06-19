# OpenWrt 流量监控系统

基于 OpenWrt 平台的实时网络流量监控工具，使用 C 语言开发底层数据捕获，通过 HTTP 接口提供 JSON 数据，前端实现可视化展示。

##  目录

- [功能特性](#功能特性)
- [环境要求](#环境要求)
- [编译步骤](#编译步骤)
- [运行步骤](#运行步骤)
- [API 接口](#api-接口)
- [项目结构](#项目结构)


## 功能特性

-  实时捕获网络数据包（基于 libpcap）
-  统计每个 IP 的接收/发送累计流量
-  计算流量峰值速率
-  计算过去 2s、10s、40s 平均速率
-  命令行实时输出统计结果
-  HTTP 接口返回 JSON 格式数据
-  Web 前端可视化（表格 + 多折线图）
-  支持交叉编译部署到 OpenWrt


## 环境要求

### 开发环境
- Ubuntu 22.04 LTS（或 WSL）
- OpenWrt SDK 24.10.0
- 交叉编译工具链

### 运行环境
- OpenWrt 24.10.0（x86_64 架构）
- libpcap 运行时库

## 编译步骤

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
### 3. 编译成功
生成 traffic_monitor 可执行文件。

## 运行步骤
### 1. 传输到 OpenWrt
```bash
cd ~/traffic_monitor/build
python3 -m http.server 8000
```
在 OpenWrt 中：
```bash
wget http://<Ubuntu_IP>:8000/traffic_monitor -O /root/traffic_monitor
chmod +x /root/traffic_monitor
```

### 2. 运行
```bash
/root/traffic_monitor
```
API 接口
```bash
curl http://<OpenWrt_IP>:8080/stats
```
## 项目结构
```text
traffic_monitor/
├── src/
│   └── traffic_stats_http.c   # 主程序（捕获+统计+HTTP服务）
├── build/                      # 编译输出目录
│   └── traffic_monitor         # 可执行文件
├── index.html                  # 前端可视化页面
├── CMakeLists.txt              # CMake 配置
├── toolchain.cmake             # 交叉编译配置
└── README.md                   # 项目说明
```
