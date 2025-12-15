# FluentMySQL Client

> 🚀 一个采用现代 C++23 标准构建的轻量级、高性能 MySQL 桌面客户端

**FluentMySQL** 是一款专为 Windows 平台设计的 MySQL 数据库管理工具，采用原生 Win32 API 和 RichEdit 控件打造流畅的用户体验。项目充分利用 C++23 的最新特性，提供直观的 SQL 查询界面和强大的数据库操作能力。

![Version](https://img.shields.io/badge/version-2025.12.14-blue)
![C++](https://img.shields.io/badge/C%2B%2B-23-brightgreen)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![License](https://img.shields.io/badge/license-MIT-orange)

---

## 📸 界面预览

```
┌─────────────────────────────────────────────────────────┐
│  FluentMySQL Client - 简洁、高效、专业                  │
├─────────────────────────────────────────────────────────┤
│  SQL 命令 (按 F5 执行):                                 │
│  ┌───────────────────────────────────────────────────┐  │
│  │ SELECT * FROM users WHERE status = 'active';     │  │
│  │                                                   │  │
│  └───────────────────────────────────────────────────┘  │
│  [执行(F5)] [清空输入] [清空输出] [连接] [断开]        │
│  ─────────────────────────────────────────────────────  │
│  输出结果:                                              │
│  ┌───────────────────────────────────────────────────┐  │
│  │ [2025-01-15 14:32:10.523]                        │  │
│  │ +----+----------+----------------------+          │  │
│  │ | id | name     | email                |          │  │
│  │ +----+----------+----------------------+          │  │
│  │ | 1  | 张三     | zhang@example.com    |          │  │
│  │ | 2  | 李四     | li@example.com       |          │  │
│  │ +----+----------+----------------------+          │  │
│  │ 共 2 行                                           │  │
│  └───────────────────────────────────────────────────┘  │
│  MySQL 状态: 已连接                                     │
└─────────────────────────────────────────────────────────┘
```

---

## ✨ 核心特性

### 🎨 用户体验
- **原生界面**：基于 Win32 API，性能卓越，资源占用低
- **DPI 自适应**：完美支持高分辨率显示器，字体自动缩放
- **实时反馈**：每条操作都有精确到毫秒的时间戳记录
- **快捷操作**：F5 执行、清空输入输出、快速连接断开

### 🔌 数据库管理
- **灵活连接**：支持自定义主机、端口、用户名、密码、数据库
- **状态监控**：实时显示连接状态，自动检测连接有效性
- **智能重连**：网络中断后自动重新连接（可配置）
- **连接池**：支持连接池管理，提升高并发场景性能

### 📊 查询功能
- **表格化显示**：自动计算列宽，对齐显示查询结果
- **多语句支持**：可批量执行多条 SQL 语句
- **结果统计**：显示影响行数、执行时间等统计信息
- **错误提示**：友好的错误信息展示，帮助快速定位问题

### 🛡️ 安全特性
- **SQL 注入检测**：内置安全检测机制，防止恶意 SQL 注入
- **参数化查询**：支持安全的参数化查询构建
- **事务管理**：完整的 ACID 事务支持（BEGIN/COMMIT/ROLLBACK）
- **字符串转义**：自动处理特殊字符，避免语法错误

### ⚡ 性能优化
- **异步查询**：非阻塞查询执行，界面响应流畅
- **结果限制**：可配置最大结果行数，避免内存溢出
- **查询统计**：记录查询次数、成功率等性能指标
- **预编译语句**：支持 PreparedStatement 提升重复查询性能

---

## 📋 目录

- [界面预览](#界面预览)
- [核心特性](#核心特性)
- [系统要求](#系统要求)
- [快速开始](#快速开始)
- [使用指南](#使用指南)
- [项目架构](#项目架构)
- [技术栈](#技术栈)
- [开发指南](#开发指南)
- [常见问题](#常见问题)
- [贡献指南](#贡献指南)
- [许可证](#许可证)

## 💻 系统要求

- **操作系统**：Windows 10 或更高版本
- **编译器**：Visual Studio 2022（支持 C++23）
- **MySQL 服务器**：MySQL 9.4 或更高版本

## 📦 依赖项

### 必需依赖

1. **MySQL Connector/C++**
   - 版本：9.5 或更高
   - 用途：MySQL 数据库连接和操作
   - 下载地址：[MySQL Connector/C++](https://dev.mysql.com/downloads/connector/cpp/)

2. **Windows SDK**
   - 版本：10.0.26100.0 或更高
   - 用途：Windows 原生 API 和控件

3. **C++ 标准库**
   - C++23 标准库特性：
     - `<expected>` - 错误处理
     - `<ranges>` - 范围算法
     - `<format>` - 字符串格式化
     - `<chrono>` - 时间处理

### 链接库

- `comctl32.lib` - Windows 通用控件
- `mysql-connector-c++.lib` - MySQL 连接器
- `Msftedit.dll` - RichEdit 控件

## 🔧 安装与构建

### 1. 安装 MySQL Connector/C++

```bash
# 下载并安装 MySQL Connector/C++ 9.5+
# 配置环境变量或在项目中设置包含目录和库目录
```

### 2. 配置项目

在 Visual Studio 中配置以下路径：

**包含目录**：
```
C:\Program Files\MySQL\MySQL Connector C++ 9.5\include
```

**库目录**：
```
C:\Program Files\MySQL\MySQL Connector C++ 9.5\lib64\vs14
```

**链接器 -> 输入 -> 附加依赖项**：
```
mysqlcppconn.lib
```

### 3. 编译项目

```bash
# 在 Visual Studio 中打开项目
# 选择 Release/x64 配置
# 生成解决方案（Ctrl+Shift+B）
```

### 4. 运行程序

```bash
# 确保 mysqlcppconn.dll 在可执行文件目录或系统 PATH 中
.\Test.exe
```

## 📖 使用说明

### 连接到数据库

1. 点击 **"连接"** 按钮
2. 在弹出的对话框中输入连接信息：
   - **主机地址**：MySQL 服务器地址（默认：localhost）
   - **端口**：MySQL 端口（默认：3306）
   - **用户名**：数据库用户名（默认：root）
   - **密码**：数据库密码
   - **数据库**：要连接的数据库名称（可选）
3. 点击 **"连接"** 完成连接

### 执行 SQL 命令

1. 在 **"SQL 命令"** 文本框中输入 SQL 语句
2. 点击 **"执行(F5)"** 按钮或按 **F5** 键执行
3. 在 **"输出结果"** 区域查看执行结果

### 示例 SQL 命令

```sql
-- 查询数据库列表
SHOW DATABASES;

-- 选择数据库
USE test;

-- 创建表
CREATE TABLE users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(100) UNIQUE
);

-- 插入数据
INSERT INTO users (name, email) VALUES ('张三', 'zhangsan@example.com');

-- 查询数据
SELECT * FROM users;

-- 更新数据
UPDATE users SET email = 'newemail@example.com' WHERE id = 1;

-- 删除数据
DELETE FROM users WHERE id = 1;
```

## 🎯 功能特性

### 数据库操作

- ✅ **基本查询**：SELECT、INSERT、UPDATE、DELETE
- ✅ **DDL 操作**：CREATE、ALTER、DROP、TRUNCATE
- ✅ **数据库管理**：SHOW DATABASES、SHOW TABLES、DESCRIBE
- ✅ **事务控制**：BEGIN、COMMIT、ROLLBACK
- ✅ **批量执行**：支持执行多条 SQL 语句

### 安全特性

- ✅ **SQL 注入检测**：自动检测潜在的 SQL 注入风险
- ✅ **参数化查询**：支持安全的参数化查询
- ✅ **字符串转义**：自动转义特殊字符
- ✅ **连接超时**：支持设置连接和查询超时时间

### 高级功能

- ✅ **连接池**：支持连接池管理，提升性能
- ✅ **自动重连**：网络中断后自动重新连接
- ✅ **查询统计**：记录查询次数、成功率等统计信息
- ✅ **日志回调**：支持自定义日志处理函数
- ✅ **结果集限制**：支持限制查询结果行数

## 📁 项目结构

```
MySQL-Local-Client/
│
├── main.cpp              # 主程序入口和窗口过程
├── database.h            # MySQL 包装器类定义
├── database.cpp          # MySQL 包装器类实现
├── render.hpp            # UI 渲染和控件管理
├── def.h                 # 全局定义和头文件包含
├── resource.h            # 资源定义
├── Test.vcxproj          # Visual Studio 项目文件
└── README.md             # 项目说明文档
```

### 核心文件说明

#### `main.cpp`
- 程序入口点 `wWinMain`
- 主窗口消息处理 `MainWindowProc`
- SQL 执行逻辑 `ExecuteSQL`
- 连接管理 `HandleConnect`、`HandleDisconnect`
- 结果格式化 `TableFormatter`

#### `database.h` / `database.cpp`
- `MySQLWrapper` - MySQL 连接和操作的主要包装类
- `MySQLResult` - 查询结果数据结构
- `MySQLRow` - 结果集行数据
- `TransactionGuard` - RAII 风格事务管理
- `ConnectionPool` - 连接池实现
- `SQLSanitizer` - SQL 安全检测工具

#### `render.hpp`
- UI 控件创建和布局管理
- RichEdit 控件封装
- UTF-8 和 Wide String 转换
- DPI 感知和字体缩放
- 连接对话框实现

## 🚀 技术亮点

### C++23 现代特性

- **`std::expected`**：优雅的错误处理，避免异常开销
- **`std::ranges`**：声明式的范围操作
- **`std::format`**：类型安全的字符串格式化
- **`std::views::enumerate`**：枚举视图，简化索引访问

### 设计模式

- **RAII**：资源自动管理（数据库连接、事务、字体等）
- **单例模式**：全局 MySQL 连接实例
- **工厂模式**：连接池管理
- **观察者模式**：日志回调机制

### 性能优化

- **预编译语句**：支持 PreparedStatement 提升性能
- **连接池**：复用连接，减少连接开销
- **批量操作**：支持批量执行 SQL 语句
- **结果集限制**：避免大结果集导致的内存问题

### 错误处理

- **结构化异常翻译**：将 Windows SEH 异常转换为 C++ 异常
- **多层异常保护**：窗口过程、SQL 执行等关键路径均有异常处理
- **详细错误信息**：提供时间戳和上下文信息的错误日志

## 👨‍💻 开发者信息

**作者**：xiren xue  
**最后更新**：2025.12.14  
**开发环境**：Visual Studio 2022  
**编程语言**：C++23  

## 📜 许可证

本项目采用 MIT 许可证。

```
MIT License

Copyright (c) 2025 xiren xue

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 🐛 已知问题

- 暂无

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📞 联系方式

如有问题或建议，请通过以下方式联系：

- 提交 Gitee Issue
- 发送邮件至项目维护者

---

**⭐ 如果这个项目对你有帮助，请给个 Star！**

