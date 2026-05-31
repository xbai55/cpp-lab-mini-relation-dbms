#include "minidb.hpp"

#include <chrono>
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
    check(result.ok, name);
    if (!result.ok) std::cout << "       message: " << result.message << "\n";
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

static long elapsed_ms(std::chrono::steady_clock::time_point begin) {
    auto elapsed = std::chrono::steady_clock::now() - begin;
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}

static void test_bptree_pressure() {
    constexpr int kRows = 5000;
    BPlusTreeIndex idx(false);

    auto begin = std::chrono::steady_clock::now();
    bool all_inserted = true;
    for (int i = 0; i < kRows; ++i) {
        all_inserted = idx.insert(int_value(i), i * 8) && all_inserted;
    }
    long insert_time = elapsed_ms(begin);

    check(all_inserted, "B+Tree pressure inserts 5000 keys");
    long offset = -1;
    check(idx.size() == static_cast<std::size_t>(kRows), "B+Tree pressure size");
    check(idx.find(int_value(0), &offset) && offset == 0, "B+Tree pressure first key lookup");
    check(idx.find(int_value(kRows / 2), &offset) && offset == (kRows / 2) * 8, "B+Tree pressure middle key lookup");
    check(idx.find(int_value(kRows - 1), &offset) && offset == (kRows - 1) * 8, "B+Tree pressure last key lookup");
    check(insert_time < 3000, "B+Tree pressure finishes within 3 seconds");
}

static void test_string_index_pressure() {
    constexpr int kRows = 800;
    BPlusTreeIndex idx(true);

    bool all_inserted = true;
    for (int i = 0; i < kRows; ++i) {
        std::string key = "student" + std::to_string(100000 + i);
        all_inserted = idx.insert(string_value(key), i) && all_inserted;
    }

    long offset = -1;
    check(all_inserted, "string index pressure inserts 800 keys");
    check(idx.find(string_value("student100000"), &offset) && offset == 0, "string index pressure first lookup");
    check(idx.find(string_value("student100799"), &offset) && offset == 799, "string index pressure last lookup");
    check(!idx.insert(string_value("student100123"), 123), "string index pressure rejects duplicate key");
}

static void test_engine_batch_workload() {
    constexpr int kRows = 400;
    std::filesystem::remove_all("stress_data");

    auto begin = std::chrono::steady_clock::now();
    {
        DatabaseEngine engine("stress_data");
        check_ok(engine.execute_sql("create database bench"), "stress create database");
        check_ok(engine.execute_sql("use bench"), "stress use database");
        check_ok(engine.execute_sql("create table student (id int primary, name string)"), "stress create table");

        bool all_inserted = true;
        for (int i = 0; i < kRows; ++i) {
            std::string sql = "insert student values (" + std::to_string(i) + ", \"name" + std::to_string(i) + "\")";
            QueryResult inserted = engine.execute_sql(sql);
            if (!inserted.ok) std::cout << "       insert " << i << " failed: " << inserted.message << "\n";
            all_inserted = inserted.ok && all_inserted;
        }
        check(all_inserted, "stress inserts 400 rows");

        QueryResult selected = engine.execute_sql("select name from student where id = 123");
        check(selected.ok &&
              selected.rows.size() == 1 &&
              selected.rows[0].values[0].str_value == "name123",
              "stress primary-key select after batch insert");

        check_ok(engine.execute_sql("update student set name = \"updated\" where id > 350"), "stress range update");
        selected = engine.execute_sql("select name from student where id = 399");
        check(selected.ok &&
              selected.rows.size() == 1 &&
              selected.rows[0].values[0].str_value == "updated",
              "stress select updated tail row");

        check_ok(engine.execute_sql("delete student where id < 200"), "stress range delete");
        selected = engine.execute_sql("select * from student");
        check(selected.ok && selected.rows.size() == 200, "stress row count after range delete");
    }

    {
        DatabaseEngine engine("stress_data");
        check_ok(engine.execute_sql("use bench"), "stress reload database");
        QueryResult selected = engine.execute_sql("select name from student where id = 399");
        check(selected.ok &&
              selected.rows.size() == 1 &&
              selected.rows[0].values[0].str_value == "updated",
              "stress persisted data after reload");
    }

    long total_time = elapsed_ms(begin);
    check(total_time < 15000, "stress engine workload finishes within 15 seconds");
    std::filesystem::remove_all("stress_data");
}

int main() {
    test_bptree_pressure();
    test_string_index_pressure();
    test_engine_batch_workload();

    if (failed == 0) {
        std::cout << "All stress tests passed.\n";
        return 0;
    }
    std::cout << failed << " stress tests failed.\n";
    return 1;
}
