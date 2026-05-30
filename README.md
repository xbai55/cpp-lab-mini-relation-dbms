# Mini Relation DBMS

![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus)
![CMake](https://img.shields.io/badge/CMake-3.16%2B-064F8C?style=flat-square&logo=cmake)
![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey?style=flat-square)
![Architecture](https://img.shields.io/badge/Architecture-Client%2FServer-2E7D32?style=flat-square)

A course project for **Modern C++ Programming**: a lightweight relational database management system inspired by the basic interactive workflow of MySQL.

The project implements SQL parsing, file-based table storage, primary-key indexing with a B+ tree, a TCP client/server architecture, formatted CLI output, and a focused unit test program.

## Overview

Mini Relation DBMS is designed as a compact teaching-oriented database system. It does not try to replace a production database. Instead, it demonstrates how several important database components work together:

| Layer | Responsibility |
| --- | --- |
| Client | Reads SQL commands from the terminal and displays formatted results |
| Network protocol | Transfers SQL requests and serialized result sets over TCP |
| SQL parser | Converts supported SQL statements into internal statement objects |
| Execution engine | Dispatches DDL and DML operations |
| Storage engine | Stores databases, schemas, table rows, and index files on disk |
| Index module | Maintains a B+ tree index for primary-key lookup |
| Test module | Verifies parser, storage, index, protocol, and SQL behavior |

## Feature Matrix

| Category | Supported features |
| --- | --- |
| Storage | File-system based database directories, table data files, schema files, and index files |
| DDL | `create database`, `drop database`, `use`, `create table`, `drop table` |
| DML | `select`, `insert`, `delete`, `update` |
| Types | `int`, `string` with a maximum length of 256 bytes |
| Conditions | `=`, `<`, `>` in optional `where` clauses |
| Index | B+ tree primary-key index for indexed equality lookup |
| Architecture | Separate server and client programs using TCP sockets |
| Interface | MySQL-like command-line interaction and table-style output |
| Tests | Custom test executable covering core project requirements |

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
|   `-- test_main.cpp
|-- CMakeLists.txt
|-- config.md
`-- README.md
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

The target evaluation environment is Linux.

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

The build produces three executables:

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

```bash
./build/mini_db_tests
```

The test program covers:

- dynamic array behavior in `MiniVector`
- SQL parsing for DDL and DML statements
- B+ tree insertion, search, duplicate rejection, and removal
- result serialization and deserialization
- database creation, deletion, switching, and persistence
- table creation and deletion
- insert, select, update, and delete workflows
- primary-key duplicate checks
- common error-handling branches

## Design Notes

This project follows the course requirement of avoiding STL standard containers such as `vector`, `map`, `set`, `list`, `deque`, and related container adapters. A custom `MiniVector` template is used for dynamic sequence storage.

Table and column names should use lowercase English letters only, with no underscores or special characters. String values must be wrapped in double quotes, for example `"peter"`.

## Status

The current implementation covers the minimum functional requirements of the project specification:

- file-based storage
- required DDL statements
- required DML statements
- B+ tree index for primary keys
- client/server separation
- TCP communication
- command-line interaction
- self-written test program

Project reports, score sheets, and final packaging should still follow the course submission template and naming rules.

