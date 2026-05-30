# Mini Relation DBMS

这是《C++现代程序设计》课程项目代码，实现了一个对标 MySQL 基本交互方式的微型关系型数据库管理系统。

## 项目功能

- 文件系统存储：数据库、表数据和主键索引保存到本地文件。
- DDL：支持 `create database`、`drop database`、`use`、`create table`、`drop table`。
- DML：支持 `select`、`insert`、`delete`、`update`。
- 数据类型：支持 `int` 和最长 256 字节的 `string`。
- 主键索引：指定 `primary` 的列会建立 B+ 树索引。
- C/S 架构：客户端通过 TCP socket 向服务端发送 SQL，服务端返回结果集。
- CLI 交互：客户端提供类似 MySQL 的命令行输入和表格化输出。
- 单元测试：测试程序覆盖解析器、执行引擎、B+ 树、协议序列化、持久化和常见错误处理。

## 目录结构

```text
.
|-- include/
|   `-- minidb.hpp
|-- src/
|   |-- client/
|   |-- common/
|   |-- server/
|   |-- sql/
|   `-- storage/
|-- tests/
|   `-- test_main.cpp
|-- CMakeLists.txt
|-- config.md
`-- README.md
```

运行后默认生成 `data/` 目录保存数据库文件，例如：

```text
data/
`-- person/
    |-- person.schema
    |-- person.dat
    `-- person.idx
```

## 构建环境

推荐在 Linux 下构建和评测：

- C++20
- CMake 3.16+
- g++ 10+ 或 clang++ 12+

## 构建方法

```bash
cmake -S . -B build
cmake --build build
```

## 运行方法

先启动服务端：

```bash
./build/mini_db_server 9090
```

再启动客户端：

```bash
./build/mini_db_client 127.0.0.1 9090
```

示例 SQL：

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

## 测试方法

```bash
./build/mini_db_tests
```

测试程序位于 `tests/test_main.cpp`，覆盖课程任务书要求的核心功能。

## 注意事项

- 表名和列名按任务书要求使用全英文小写，不包含下划线和特殊字符。
- 字符串数据需要使用双引号，例如 `"peter"`。
- 课程任务书要求不使用 STL 标准容器，本项目使用自定义 `MiniVector` 管理动态数组。
- 当前项目面向 Linux 评测环境；Windows 下 socket 已做兼容处理，但仍建议以 Linux 构建结果为准。

