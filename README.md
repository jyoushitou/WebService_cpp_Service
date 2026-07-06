# Service — C++ 通用服务层组件

> WebServer 单体架构遗留下来的**通用服务层代码**，包含用户线程管理、任务系统和工具函数。

![C++](https://img.shields.io/badge/C++-17-%2300599C?style=flat-square&logo=c%2B%2B)
![MySQL](https://img.shields.io/badge/MySQL-8-%234479A1?style=flat-square&logo=mysql)
![Docker](https://img.shields.io/badge/Docker-24-%232496ED?style=flat-square&logo=docker)

---

## 📖 概述

Service 子仓库包含从原单体架构 C++ HTTP 服务器中提炼出的**通用核心组件**，包括：

- **用户线程管理** — 每个登录用户拥有独立的业务处理线程
- **任务系统** — 异步任务投递、处理和查询
- **工具函数** — 日志输出、JSON 解析、Token 认证

> ⚠️ **注意**：此仓库为单体架构过渡到微服务架构过程中的中间产物。其中的线程模型和任务系统已被 RPCGateway 使用，部分工具函数已存在多处副本。后续将逐步整合到统一的基础库中。

---

## 📂 项目结构

```
Service/
├── source/                        # 源代码目录（当前为空）
├── Task.h                         # 任务系统头文件
├── Task.cpp                       # 任务系统实现
├── UserThread.h                   # 用户线程管理头文件
├── UserThread.cpp                 # 用户线程管理实现
├── Utils.h                        # 通用工具头文件
├── Utils.cpp                      # 通用工具实现
└── README.md                      # 本文件
```

---

## 🧩 核心组件

### 1. 任务系统 (`Task.h` / `Task.cpp`)

任务系统提供了完整的**异步任务生命周期管理**：

```
客户端 ──投递任务──► 任务系统 ──分发──► 用户线程 ──处理──► 结果存储
                    │                                           │
                    └──── 生成 TaskID ────── 查询结果 ──────────┘
```

#### 核心功能

| 函数 | 说明 |
|------|------|
| `Generate_Task_Id()` | 生成唯一任务 ID（时间戳 + 16位随机十六进制） |
| `Post_User_Task()` | 向指定用户的线程投递任务 |
| `Update_Task_Result()` | 更新任务执行结果 |
| `Get_Task_Result()` | 根据任务 ID 查询任务结果 |

#### 任务类型枚举

| 类型 | 值 | 说明 |
|------|-----|------|
| `PING` | 0 | 心跳检测 |
| `PROCESS_DATA` | 1 | 数据处理 |
| `SEND_NOTIFICATION` | 2 | 发送通知 |
| `SYNC_DATABASE` | 3 | 数据库同步 |
| `CUSTOM_EVENT` | 4 | 自定义事件 |
| `SHUTDOWN` | 5 | 关闭线程 |

#### 任务状态枚举

| 状态 | 说明 |
|------|------|
| `PENDING` | 等待处理 |
| `PROCESSING` | 正在处理 |
| `COMPLETED` | 已完成 |
| `FAILED` | 执行失败 |

### 2. 用户线程管理 (`UserThread.h` / `UserThread.cpp`)

实现了**每用户单线程**的并发模型：

#### 核心函数

| 函数 | 说明 |
|------|------|
| `Create_User_Thread(userId, name, level)` | 为用户创建独立业务线程 |
| `User_Worker_Routine(info)` | 用户线程主循环（等待任务 → 处理任务 → 空闲心跳） |

#### 线程工作流程

```
线程启动
  │
  ▼
创建独立 MySQL 连接
  │
  ▼
等待任务（最多5秒超时）
  │
  ├── 有任务到达 → 逐一处理队列任务
  │     ├── PING → 返回 pong
  │     ├── PROCESS_DATA → 数据处理 + 数据库保存
  │     ├── SEND_NOTIFICATION → 发送通知
  │     ├── SYNC_DATABASE → 数据库同步
  │     ├── CUSTOM_EVENT → 自定义事件回显
  │     └── SHUTDOWN → 线程退出
  │
  └── 超时无任务 → 后台清理（30秒心跳输出）
  │
  ▼
线程退出 → 关闭数据库连接
```

### 3. 工具函数 (`Utils.h` / `Utils.cpp`)

#### 日志输出

| 函数 | 前缀 | 输出流 |
|------|------|--------|
| `Out_System()` | `[输出]` | stdout |
| `Out_System_Mysql()` | `[MySQL]` | stdout |
| `Out_System_Http()` | `[HTTP]` | stdout |
| `Out_System_Error()` | `[error]` | stderr |
| `Out_System_Exit()` | `[exit]` | stdout |
| `Out_System_user()` | `[用户名]` | stdout |

#### JSON 解析（`Utils::Json` 子命名空间）

| 函数 | 说明 |
|------|------|
| `Parse_Json_String(body, key)` | 解析 JSON 字符串中指定 key 的字符串值 |
| `Parse_Json_Value_Raw(body, key)` | 解析 JSON 中指定 key 的原始值（支持数字/布尔/嵌套） |

> 纯手写解析，无第三方 JSON 库依赖。

#### Token 认证（`Utils::Auth` 子命名空间）

| 函数/结构体 | 说明 |
|-------------|------|
| `Generate_Token()` | 生成 32 位随机十六进制 Token |
| `Get_Token_From_Request()` | 从 `Authorization` 头提取 Token |
| `Validate_Token()` | 验证 Token 并返回 SessionInfo |
| `SessionInfo` | 会话信息（userId, level, name, device） |
| `DeviceInfo` | 设备信息（deviceName, userAgent, clientIp, loginTime） |
| `g_tokenStore` | 全局 Token 存储表（`unordered_map`） |

---

## 🔗 依赖关系

```

RPCGateway ──依赖──► Service 组件
                        │
                        ├── Task.h/cpp
                        ├── UserThread.h/cpp
                        └── Utils.h/cpp
                               │
                               └── MySQL 子仓库（MyMySQL.h）
```

RPCGateway 直接引用了 Service 中的：
- `Task.h` / `Task.cpp` — 任务系统
- `UserThread.h` / `UserThread.cpp` — 用户线程管理
- `Utils.h` / `Utils.cpp` — 工具函数

---

## 🛠️ 构建说明
Service 组件通常作为 RPCGateway 的内联代码一起编译，不单独构建。

如需独立编译，可以参考以下 CMake 配置：

```cmake
cmake_minimum_required(VERSION 3.10)
project(Service VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(service_lib STATIC
    Task.cpp
    UserThread.cpp
    Utils.cpp
)

target_include_directories(service_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
```

---

## 📋 后续规划

- [ ] **重构为独立基础库** — 消除与 RPCGateway 和 MySQL 仓库中的代码重复
- [ ] **提取通用工具为独立模块** — Utils、Auth、Json 拆分为独立组件
- [ ] **线程模型标准化** — 适配 gRPC 异步模型
- [ ] **单元测试覆盖** — 为任务系统编写完整的单元测试

---

## 📬 联系方式

- 仓库地址：[https://github.com/jyoushitou/cpp_Service.git](https://github.com/jyoushitou/cpp_Service.git)
- 父项目：[WebServer](https://github.com/jyoushitou/WebServer)
