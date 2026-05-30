# Mini Relation DBMS

![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus)
![CMake](https://img.shields.io/badge/CMake-3.16%2B-064F8C?style=flat-square&logo=cmake)
![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey?style=flat-square)
![Architecture](https://img.shields.io/badge/Architecture-Client%2FServer-2E7D32?style=flat-square)

**中文** | [English](README_EN.md)

## 项目简介

Mini Relation DBMS 是《C++现代程序设计》课程项目，实现了一个对标 MySQL 基本交互方式的微型关系型数据库管理系统。

项目包含 SQL 解析、文件式表存储、基于 B+ 树的主键索引、TCP 客户端/服务端通信、命令行交互界面以及自编写测试程序。它不是生产级数据库，而是一个用于展示数据库核心模块如何协同工作的教学型系统。

## 架构概览

| 模块 | 主要职责 |
| --- | --- |
| 客户端 | 读取用户输入的 SQL，并格式化显示服务端返回结果 |
| 通信协议 | 通过 TCP 传输 SQL 请求和序列化后的结果集 |
| SQL 解析器 | 将支持的 SQL 语句转换为内部语句对象 |
| 执行引擎 | 分发并执行 DDL、DML 操作 |
| 存储引擎 | 管理数据库目录、表结构文件、数据文件和索引文件 |
| 索引模块 | 使用 B+ 树维护主键索引，加速主键等值查询 |
| 测试模块 | 验证解析、存储、索引、协议和 SQL 执行逻辑 |

## 功能矩阵

| 类别 | 已支持功能 |
| --- | --- |
| 文件存储 | 数据库目录、表结构文件、表数据文件、索引文件 |
| DDL | `create database`、`drop database`、`use`、`create table`、`drop table` |
| DML | `select`、`insert`、`delete`、`update` |
| 数据类型 | `int`、最长 256 字节的 `string` |
| 条件表达式 | `where` 子句支持 `=`、`<`、`>` |
| 主键索引 | 主键列自动建立 B+ 树索引 |
| 系统架构 | 服务端和客户端分离，使用 TCP socket 通信 |
| 交互界面 | 类 MySQL 的命令行输入和表格化输出 |
| 测试程序 | 覆盖课程任务书要求的核心功能 |

## 支持的 SQL

```sql
create database <dbname>
drop database <dbname>
use <dbname>

create table <table> (
    <column> <type> [primary],
    ...
)
drop table <table>

insert <table> values (<value>, ...)
select <column | *> from <table> [where <column> <op> <value>]
delete <table> [where <column> <op> <value>]
update <table> set <column> = <value> [where <column> <op> <value>]
exit
```

示例：

```sql
create database person
use person
create table person (id int primary, name string)
insert person values (1001, "peter")
select name from person where id = 1001
update person set name = "tom" where id = 1001
delete person where id = 1001
drop table person
drop database person
exit
```

## 项目结构

```text
.
|-- include/
|   `-- minidb.hpp
|-- src/
|   |-- client/
|   |   `-- main.cpp
|   |-- common/
|   |   `-- protocol.cpp
|   |-- server/
|   |   `-- main.cpp
|   |-- sql/
|   |   |-- executor.cpp
|   |   `-- parser.cpp
|   `-- storage/
|       |-- bptree.cpp
|       `-- database.cpp
|-- tests/
|   `-- test_main.cpp
|-- CMakeLists.txt
|-- config.md
|-- README.md
`-- README_EN.md
```

运行后默认在 `data/` 目录中保存数据库文件：

```text
data/
`-- person/
    |-- person.schema
    |-- person.dat
    `-- person.idx
```

## 构建环境

目标评测环境为 Linux。

| 要求 | 版本 |
| --- | --- |
| C++ 标准 | C++20 |
| 构建工具 | CMake 3.16 或更高版本 |
| 编译器 | g++ 10+ 或 clang++ 12+ |
| 网络接口 | TCP socket |

## 构建方法

```bash
cmake -S . -B build
cmake --build build
```

构建后会生成：

| 目标文件 | 说明 |
| --- | --- |
| `mini_db_server` | 数据库服务端 |
| `mini_db_client` | 交互式 SQL 客户端 |
| `mini_db_tests` | 单元测试与集成测试程序 |

## 运行方法

先启动服务端：

```bash
./build/mini_db_server 9090
```

再在另一个终端启动客户端：

```bash
./build/mini_db_client 127.0.0.1 9090
```

随后即可在客户端提示符中输入 SQL 语句。

## 测试方法

```bash
./build/mini_db_tests
```

测试程序覆盖：

- `MiniVector` 动态数组行为
- DDL、DML 语句解析
- B+ 树插入、查找、重复键拒绝和删除
- 结果集序列化与反序列化
- 数据库创建、删除、切换和持久化
- 表创建、表删除、插入、查询、更新和删除流程
- 主键重复检查
- 常见错误处理分支

## 设计说明

本项目遵循课程任务书要求，不使用 `vector`、`map`、`set`、`list`、`deque` 等 STL 标准容器，而是使用自定义 `MiniVector` 模板管理动态序列。

表名和列名应使用全英文小写字母，不包含下划线和特殊字符。字符串值需要使用双引号，例如 `"peter"`。

## 当前状态

当前实现已覆盖任务书中的最低功能要求：

- 文件系统存储
- 必需的 DDL 语句
- 必需的 DML 语句
- 主键 B+ 树索引
- 客户端/服务端分离
- TCP 通信
- 命令行交互
- 自编写测试程序

项目报告、评分表和最终压缩包仍需按照课程模板和命名规则整理。

