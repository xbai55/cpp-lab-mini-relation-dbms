#ifndef MINI_DB_HPP
#define MINI_DB_HPP

#include <cstddef>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace minidb {

#ifdef _WIN32
using SocketHandle = std::uintptr_t;
#else
using SocketHandle = int;
#endif

template <typename T>
class MiniVector {
public:
    MiniVector() : data_(nullptr), size_(0), capacity_(0) {}
    MiniVector(const MiniVector& other) : data_(nullptr), size_(0), capacity_(0) {
        reserve(other.size_);
        for (std::size_t i = 0; i < other.size_; ++i) push_back(other.data_[i]);
    }
    MiniVector& operator=(const MiniVector& other) {
        if (this == &other) return *this;
        clear();
        reserve(other.size_);
        for (std::size_t i = 0; i < other.size_; ++i) push_back(other.data_[i]);
        return *this;
    }
    ~MiniVector() { delete[] data_; }

    void push_back(const T& value) {
        if (size_ == capacity_) reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        data_[size_++] = value;
    }
    void erase(std::size_t pos) {
        if (pos >= size_) return;
        for (std::size_t i = pos; i + 1 < size_; ++i) data_[i] = data_[i + 1];
        --size_;
    }
    void clear() { size_ = 0; }
    void reserve(std::size_t cap) {
        if (cap <= capacity_) return;
        T* next = new T[cap];
        for (std::size_t i = 0; i < size_; ++i) next[i] = data_[i];
        delete[] data_;
        data_ = next;
        capacity_ = cap;
    }
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    T& operator[](std::size_t i) { return data_[i]; }
    const T& operator[](std::size_t i) const { return data_[i]; }

private:
    T* data_;
    std::size_t size_;
    std::size_t capacity_;
};

enum class ColumnType { Int, String };
enum class OpType { None, Eq, Lt, Gt };
enum class StatementType {
    Invalid, CreateDatabase, DropDatabase, UseDatabase, CreateTable,
    DropTable, Insert, Select, DeleteRows, Update, Exit
};

struct ColumnDef {
    std::string name;
    ColumnType type = ColumnType::String;
    bool primary = false;
};

struct Value {
    ColumnType type = ColumnType::String;
    int int_value = 0;
    std::string str_value;
};

struct Condition {
    bool has = false;
    std::string column;
    OpType op = OpType::None;
    Value value;
};

struct Row {
    MiniVector<Value> values;
};

struct QueryResult {
    bool ok = true;
    std::string message;
    MiniVector<std::string> columns;
    MiniVector<Row> rows;
};

struct Statement {
    StatementType type = StatementType::Invalid;
    std::string database;
    std::string table;
    std::string select_column;
    std::string set_column;
    Value set_value;
    Condition condition;
    MiniVector<ColumnDef> columns;
    MiniVector<Value> values;
};

bool is_valid_name(const std::string& name);
std::string trim(const std::string& s);
std::string lower_copy(const std::string& s);
std::string value_to_string(const Value& v);
bool compare_values(const Value& left, OpType op, const Value& right);

class BPlusTreeIndex {
public:
    explicit BPlusTreeIndex(bool string_key = false);
    BPlusTreeIndex(const BPlusTreeIndex&) = delete;
    BPlusTreeIndex& operator=(const BPlusTreeIndex&) = delete;
    ~BPlusTreeIndex();
    bool insert(const Value& key, long offset);
    bool find(const Value& key, long* offset) const;
    bool remove(const Value& key);
    void clear();
    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;
    std::size_t size() const { return size_; }

private:
    struct Node {
        bool leaf;
        MiniVector<Value> keys;
        MiniVector<long> offsets;
        MiniVector<Node*> children;
        Node* next;
        explicit Node(bool is_leaf) : leaf(is_leaf), next(nullptr) {}
    };
    static constexpr std::size_t ORDER = 4;
    bool string_key_;
    Node* root_;
    std::size_t size_;
    int compare_key(const Value& a, const Value& b) const;
    std::size_t lower_bound_key(const MiniVector<Value>& keys, const Value& key) const;
    void clear_node(Node* node);
    Node* find_leaf(const Value& key) const;
    bool insert_recursive(Node* node, const Value& key, long offset, Value& promote_key, Node*& promote_child);
    void insert_into_parent(Node* parent, std::size_t child_pos, const Value& key, Node* child);
    void save_leaf_chain(std::ofstream& out) const;
};

class DatabaseEngine {
public:
    explicit DatabaseEngine(std::filesystem::path root = "data");
    QueryResult execute(const Statement& stmt);
    QueryResult execute_sql(const std::string& sql);

private:
    std::filesystem::path root_;
    std::string current_db_;

    QueryResult create_database(const std::string& name);
    QueryResult drop_database(const std::string& name);
    QueryResult use_database(const std::string& name);
    QueryResult create_table(const Statement& stmt);
    QueryResult drop_table(const std::string& table);
    QueryResult insert_row(const Statement& stmt);
    QueryResult select_rows(const Statement& stmt);
    QueryResult delete_rows(const Statement& stmt);
    QueryResult update_rows(const Statement& stmt);

    std::filesystem::path db_path() const;
    std::filesystem::path table_base(const std::string& table) const;
    bool require_db(QueryResult& result) const;
    bool load_schema(const std::string& table, MiniVector<ColumnDef>& columns) const;
    bool save_schema(const std::string& table, const MiniVector<ColumnDef>& columns);
    bool load_rows(const std::string& table, const MiniVector<ColumnDef>& columns, MiniVector<Row>& rows) const;
    bool save_rows(const std::string& table, const MiniVector<ColumnDef>& columns, const MiniVector<Row>& rows);
    int column_index(const MiniVector<ColumnDef>& columns, const std::string& name) const;
    int primary_index(const MiniVector<ColumnDef>& columns) const;
    bool rebuild_index(const std::string& table, const MiniVector<ColumnDef>& columns, const MiniVector<Row>& rows);
    bool row_matches(const Row& row, const MiniVector<ColumnDef>& columns, const Condition& cond) const;
};

class Parser {
public:
    Statement parse(const std::string& sql);

private:
    MiniVector<std::string> tokens_;
    std::size_t pos_ = 0;

    void tokenize(const std::string& sql);
    bool accept(const std::string& token);
    std::string expect_word(const std::string& what);
    void expect(const std::string& token);
    bool end() const;
    std::string peek() const;
    std::string next();
    Value parse_value();
    Condition parse_optional_where();
};

std::string encode_result(const QueryResult& result);
QueryResult decode_result(const std::string& text);
std::string format_result_table(const QueryResult& result);

bool send_message(SocketHandle fd, const std::string& payload);
bool recv_message(SocketHandle fd, std::string& payload);

} // namespace minidb

#endif
