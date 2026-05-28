# Mini Relation DBMS 运行环境配置

## 目标评测环境

- 操作系统：Linux
- C++ 标准：C++20
- 构建工具：CMake 3.16 或更高版本
- 编译器：g++ 10 或更高版本 / clang++ 12 或更高版本
- 网络通信：TCP socket

## 构建方式

```bash
cmake -S . -B build
cmake --build build
```

## 运行方式

先启动服务端：

```bash
./build/mini_db_server 9090
```

再启动客户端：

```bash
./build/mini_db_client 127.0.0.1 9090
```

## 测试方式

```bash
./build/mini_db_tests
```

## 功能说明

数据默认保存在当前工作目录下的 `data/` 目录中。工程实现了课程任务书要求的 DDL、DML、文件存储、主键 B+树索引、C/S 通信和命令行交互界面。测试程序位于 `tests/test_main.cpp`，覆盖建库、删库、切库、建表、删表、插入、查询、删除、更新、主键重复、字符串主键、持久化、结果序列化和常见错误处理。

