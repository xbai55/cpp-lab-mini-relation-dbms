#include "minidb.hpp"

#include <filesystem>

using namespace minidb;

static int failed = 0;

static void check(bool cond, const std::string& name) {
    if (cond) {
        std::cout << "[PASS] " << name << "\n";
    } else {
        std::cout << "[FAIL] " << name << "\n";
        ++failed;
    }
}

static void check_ok(const QueryResult& result, const std::string& name) {
    check(result.ok, name + " ok");
    if (!result.ok) std::cout << "       message: " << result.message << "\n";
}

static void check_fail(const QueryResult& result, const std::string& name) {
    check(!result.ok, name + " rejected");
}

static Value int_value(int n) {
    Value v;
    v.type = ColumnType::Int;
    v.int_value = n;
    return v;
}

static Value string_value(const std::string& s) {
    Value v;
    v.type = ColumnType::String;
    v.str_value = s;
    return v;
}

static void test_vector() {
    MiniVector<int> v;
    for (int i = 0; i < 10; ++i) v.push_back(i);
    v.erase(4);
    check(v.size() == 9 && v[4] == 5 && v[8] == 9, "MiniVector grows and erases");

    MiniVector<int> copy = v;
    v.clear();
    check(copy.size() == 9 && v.empty(), "MiniVector copy is independent");
}

static void test_parser() {
    Parser p;
    Statement create = p.parse("create table person (id int primary, name string)");
    check(create.type == StatementType::CreateTable &&
          create.table == "person" &&
          create.columns.size() == 2 &&
          create.columns[0].primary,
          "parse create table with primary key");

    Statement select = p.parse("select name from person where id = 1001");
    check(select.type == StatementType::Select &&
          select.select_column == "name" &&
          select.condition.has &&
          select.condition.op == OpType::Eq,
          "parse select where");

    Statement update = p.parse("update person set name = \"Tom\" where id > 1000");
    check(update.type == StatementType::Update &&
          update.set_column == "name" &&
          update.set_value.str_value == "Tom" &&
          update.condition.op == OpType::Gt,
          "parse update where");

    try {
        p.parse("create table bad_name (id int)");
        check(false, "reject invalid identifier token");
    } catch (const std::exception&) {
        check(true, "reject invalid identifier token");
    }
}

static void test_bptree() {
    BPlusTreeIndex idx(false);
    for (int i = 20; i >= 1; --i) {
        check(idx.insert(int_value(i), i * 10), "B+Tree insert " + std::to_string(i));
    }
    check(!idx.insert(int_value(7), 700), "B+Tree rejects duplicate int key");

    long offset = -1;
    check(idx.find(int_value(1), &offset) && offset == 10, "B+Tree finds first key");
    check(idx.find(int_value(20), &offset) && offset == 200, "B+Tree finds last key");
    check(idx.remove(int_value(10)) && !idx.find(int_value(10), &offset), "B+Tree removes key");

    BPlusTreeIndex str_idx(true);
    check(str_idx.insert(string_value("alice"), 1) &&
          str_idx.insert(string_value("peter"), 2) &&
          !str_idx.insert(string_value("alice"), 3),
          "B+Tree handles string primary keys");
}

static void test_protocol() {
    QueryResult result;
    result.message = "Query OK | escaped";
    result.columns.push_back("note");
    Row row;
    row.values.push_back(string_value("pipe|slash\\line\nend"));
    result.rows.push_back(row);

    QueryResult decoded = decode_result(encode_result(result));
    check(decoded.ok &&
          decoded.message == result.message &&
          decoded.rows.size() == 1 &&
          decoded.rows[0].values[0].str_value == row.values[0].str_value,
          "protocol preserves escaped result fields");
}

static void test_engine_core_sql() {
    std::filesystem::remove_all("test_data");
    DatabaseEngine engine("test_data");

    check_ok(engine.execute_sql("create database person"), "create database");
    check_fail(engine.execute_sql("create database person"), "duplicate database");
    check_ok(engine.execute_sql("use person"), "use database");
    check_ok(engine.execute_sql("create table person (id int primary, name string)"), "create table");
    check_fail(engine.execute_sql("create table person (id int primary, name string)"), "duplicate table");

    check_ok(engine.execute_sql("insert person values (1001, \"peter\")"), "insert row 1");
    check_ok(engine.execute_sql("insert person values (1002, \"alice\")"), "insert row 2");
    check_ok(engine.execute_sql("insert person values (1003, \"bob\")"), "insert row 3");
    check_fail(engine.execute_sql("insert person values (1001, \"again\")"), "duplicate primary key");
    check_fail(engine.execute_sql("insert person values (\"wrong\", \"type\")"), "type mismatch insert");

    QueryResult selected = engine.execute_sql("select name from person where id = 1001");
    check(selected.ok &&
          selected.columns.size() == 1 &&
          selected.rows.size() == 1 &&
          selected.rows[0].values[0].str_value == "peter",
          "select one column by primary index");

    selected = engine.execute_sql("select * from person where id > 1001");
    check(selected.ok && selected.columns.size() == 2 && selected.rows.size() == 2, "select all with greater-than condition");

    check_ok(engine.execute_sql("update person set name = \"tom\" where id = 1001"), "update with where");
    selected = engine.execute_sql("select name from person where id = 1001");
    check(selected.ok && selected.rows.size() == 1 && selected.rows[0].values[0].str_value == "tom", "select updated value");

    check_fail(engine.execute_sql("update person set id = 1003 where id = 1002"), "reject duplicate primary key after update");
    selected = engine.execute_sql("select name from person where id = 1002");
    check(selected.ok && selected.rows.size() == 1 && selected.rows[0].values[0].str_value == "alice", "failed update leaves table unchanged");

    check_ok(engine.execute_sql("delete person where id < 1003"), "delete with where");
    selected = engine.execute_sql("select * from person");
    check(selected.ok && selected.rows.size() == 1 && selected.rows[0].values[0].int_value == 1003, "select remaining rows");

    check_ok(engine.execute_sql("delete person"), "delete without where");
    selected = engine.execute_sql("select * from person");
    check(selected.ok && selected.rows.empty(), "delete all rows");

    check_ok(engine.execute_sql("drop table person"), "drop table");
    check_fail(engine.execute_sql("select * from person"), "select dropped table");
    check_ok(engine.execute_sql("drop database person"), "drop database");
    std::filesystem::remove_all("test_data");
}

static void test_engine_string_primary_and_persistence() {
    std::filesystem::remove_all("test_data");
    {
        DatabaseEngine engine("test_data");
        check_ok(engine.execute_sql("create database school"), "create persistence database");
        check_ok(engine.execute_sql("use school"), "use persistence database");
        check_ok(engine.execute_sql("create table student (sid string primary, score int)"), "create string-primary table");
        check_ok(engine.execute_sql("insert student values (\"s001\", 90)"), "insert string primary row");
        check_ok(engine.execute_sql("insert student values (\"s002\", 95)"), "insert second string primary row");
        check_fail(engine.execute_sql("insert student values (\"s001\", 88)"), "reject duplicate string primary");
    }
    {
        DatabaseEngine engine("test_data");
        check_ok(engine.execute_sql("use school"), "reuse persisted database");
        QueryResult selected = engine.execute_sql("select score from student where sid = \"s002\"");
        check(selected.ok &&
              selected.rows.size() == 1 &&
              selected.rows[0].values[0].int_value == 95,
              "load persisted rows and index");
    }
    std::filesystem::remove_all("test_data");
}

static void test_error_handling() {
    std::filesystem::remove_all("test_data");
    DatabaseEngine engine("test_data");

    check_fail(engine.execute_sql("create table person (id int)"), "create table without database");
    check_fail(engine.execute_sql("use missing"), "use missing database");
    check_fail(engine.execute_sql("create database Bad"), "reject uppercase database name");

    check_ok(engine.execute_sql("create database err"), "create error database");
    check_ok(engine.execute_sql("use err"), "use error database");
    check_fail(engine.execute_sql("create table bad (id float)"), "reject unsupported type");
    check_ok(engine.execute_sql("create table person (id int primary, name string)"), "create table for error tests");
    check_fail(engine.execute_sql("select age from person"), "reject missing select column");
    check_fail(engine.execute_sql("select * from person where age = 1"), "reject missing where column");
    check_fail(engine.execute_sql("update person set age = 1"), "reject missing set column");

    std::filesystem::remove_all("test_data");
}

int main() {
    test_vector();
    test_parser();
    test_bptree();
    test_protocol();
    test_engine_core_sql();
    test_engine_string_primary_and_persistence();
    test_error_handling();

    if (failed == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    }
    std::cout << failed << " tests failed.\n";
    return 1;
}
