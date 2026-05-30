# Mini Relation DBMS

![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus)
![CMake](https://img.shields.io/badge/CMake-3.16%2B-064F8C?style=flat-square&logo=cmake)
![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey?style=flat-square)
![Architecture](https://img.shields.io/badge/Architecture-Client%2FServer-2E7D32?style=flat-square)

[中文](README.md) | **English**

## Introduction

Mini Relation DBMS is a lightweight relational database management system that provides a MySQL-like command-line SQL experience.

The project includes SQL parsing, file-based table storage, primary-key indexing with a B+ tree, TCP client/server communication, a command-line interface, and an automated test workflow. Its modules are organized around core database capabilities, making the system straightforward to build, run, test, and extend.

## Architecture Overview

| Module | Responsibility |
| --- | --- |
| Client | Reads SQL commands and displays formatted server results |
| Protocol | Transfers SQL requests and serialized result sets over TCP |
| SQL parser | Converts supported SQL statements into internal statement objects |
| Execution engine | Dispatches and executes DDL and DML operations |
| Storage engine | Manages database directories, schema files, data files, and index files |
| Index module | Maintains a B+ tree primary-key index |
| Test module | Verifies parser, storage, index, protocol, and SQL behavior |

## Feature Matrix

| Category | Supported features |
| --- | --- |
| Storage | Database directories, schema files, table data files, and index files |
| DDL | `create database`, `drop database`, `use`, `create table`, `drop table` |
| DML | `select`, `insert`, `delete`, `update` |
| Types | `int`, `string` with a maximum length of 256 bytes |
| Conditions | `=`, `<`, `>` in optional `where` clauses |
| Index | B+ tree index for primary-key columns |
| Architecture | Separate server and client using TCP sockets |
| Interface | MySQL-like command-line input and table-style output |
| Tests | Unit tests and client/server smoke test |

## Supported SQL

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

Example:

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

## Project Structure

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
|   |-- smoke_client_server.sh
|   `-- test_main.cpp
|-- CMakeLists.txt
|-- config.md
|-- README.md
`-- README_EN.md
```

Runtime data is stored under `data/` by default:

```text
data/
`-- person/
    |-- person.schema
    |-- person.dat
    `-- person.idx
```

## Build Requirements

Linux is the recommended build and runtime environment.

| Requirement | Version |
| --- | --- |
| C++ standard | C++20 |
| Build system | CMake 3.16 or later |
| Compiler | g++ 10+ or clang++ 12+ |
| Network API | TCP socket |

## Build

```bash
cmake -S . -B build
cmake --build build
```

The build produces:

| Target | Description |
| --- | --- |
| `mini_db_server` | Database server |
| `mini_db_client` | Interactive SQL client |
| `mini_db_tests` | Unit and integration test executable |

## Run

Start the server:

```bash
./build/mini_db_server 9090
```

Start the client in another terminal:

```bash
./build/mini_db_client 127.0.0.1 9090
```

Then enter SQL commands in the client prompt.

## Test

The recommended way is to use CTest to run both the unit tests and the client/server smoke test:

```bash
ctest --test-dir build --output-on-failure
```

You can also run the core test executable directly:

```bash
./build/mini_db_tests
```

The tests cover:

- `MiniVector` dynamic array behavior
- DDL and DML parsing
- B+ tree insertion, search, duplicate rejection, and removal
- result serialization and deserialization
- database creation, deletion, switching, and persistence
- table creation, table deletion, insert, select, update, and delete workflows
- primary-key duplicate checks
- common error-handling branches

The script `tests/smoke_client_server.sh` starts the server automatically, sends a SQL session through the client, and checks the output after query, update, and delete operations.

## Design Notes

The project uses a custom `MiniVector` template for dynamic sequence storage and avoids relying on STL standard containers in core data structures. Table and column names should use lowercase English letters only, with no underscores or special characters. String values must be wrapped in double quotes, for example `"peter"`.

## Status

The current implementation includes:

- file-based storage
- DDL and DML execution
- B+ tree index for primary keys
- client/server separation
- TCP communication
- command-line interaction
- automated tests
