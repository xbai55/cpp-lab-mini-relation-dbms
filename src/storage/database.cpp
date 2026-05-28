#include "minidb.hpp"

namespace minidb {

DatabaseEngine::DatabaseEngine(std::filesystem::path root) : root_(std::move(root)) {
    std::filesystem::create_directories(root_);
}

std::filesystem::path DatabaseEngine::db_path() const {
    return root_ / current_db_;
}

std::filesystem::path DatabaseEngine::table_base(const std::string& table) const {
    return db_path() / table;
}

bool DatabaseEngine::require_db(QueryResult& result) const {
    if (current_db_.empty()) {
        result.ok = false;
        result.message = "no database selected";
        return false;
    }
    if (!std::filesystem::exists(db_path())) {
        result.ok = false;
        result.message = "current database does not exist";
        return false;
    }
    return true;
}

QueryResult DatabaseEngine::create_database(const std::string& name) {
    QueryResult r;
    if (!is_valid_name(name)) {
        r.ok = false;
        r.message = "invalid database name, use lowercase letters only";
        return r;
    }
    std::filesystem::path path = root_ / name;
    if (std::filesystem::exists(path)) {
        r.ok = false;
        r.message = "database already exists";
        return r;
    }
    std::filesystem::create_directories(path);
    r.message = "Query OK, database created";
    return r;
}

QueryResult DatabaseEngine::drop_database(const std::string& name) {
    QueryResult r;
    if (!is_valid_name(name)) {
        r.ok = false;
        r.message = "invalid database name";
        return r;
    }
    std::filesystem::path path = root_ / name;
    if (!std::filesystem::exists(path)) {
        r.ok = false;
        r.message = "database does not exist";
        return r;
    }
    std::filesystem::remove_all(path);
    if (current_db_ == name) current_db_.clear();
    r.message = "Query OK, database dropped";
    return r;
}

QueryResult DatabaseEngine::use_database(const std::string& name) {
    QueryResult r;
    if (!std::filesystem::exists(root_ / name)) {
        r.ok = false;
        r.message = "database does not exist";
        return r;
    }
    current_db_ = name;
    r.message = "Database changed";
    return r;
}

bool DatabaseEngine::save_schema(const std::string& table, const MiniVector<ColumnDef>& columns) {
    std::ofstream out(table_base(table).string() + ".schema", std::ios::trunc);
    if (!out.good()) return false;
    for (std::size_t i = 0; i < columns.size(); ++i) {
        out << columns[i].name << "\t"
            << (columns[i].type == ColumnType::Int ? "int" : "string") << "\t"
            << (columns[i].primary ? "primary" : "-") << "\n";
    }
    return true;
}

bool DatabaseEngine::load_schema(const std::string& table, MiniVector<ColumnDef>& columns) const {
    columns.clear();
    std::ifstream in(table_base(table).string() + ".schema");
    if (!in.good()) return false;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string type, primary;
        ColumnDef col;
        if (!std::getline(ss, col.name, '\t')) continue;
        if (!std::getline(ss, type, '\t')) continue;
        if (!std::getline(ss, primary)) continue;
        col.type = type == "int" ? ColumnType::Int : ColumnType::String;
        col.primary = primary == "primary";
        columns.push_back(col);
    }
    return true;
}

int DatabaseEngine::column_index(const MiniVector<ColumnDef>& columns, const std::string& name) const {
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

int DatabaseEngine::primary_index(const MiniVector<ColumnDef>& columns) const {
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].primary) return static_cast<int>(i);
    }
    return -1;
}

static std::string encode_cell(const Value& v) {
    std::string s = value_to_string(v);
    std::string out;
    for (char ch : s) {
        if (ch == '\\' || ch == '\t' || ch == '\n') out.push_back('\\');
        if (ch == '\t') out.push_back('t');
        else if (ch == '\n') out.push_back('n');
        else out.push_back(ch);
    }
    return out;
}

static std::string decode_cell(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            if (s[i] == 't') out.push_back('\t');
            else if (s[i] == 'n') out.push_back('\n');
            else out.push_back(s[i]);
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

bool DatabaseEngine::save_rows(const std::string& table, const MiniVector<ColumnDef>& columns, const MiniVector<Row>& rows) {
    std::ofstream out(table_base(table).string() + ".dat", std::ios::trunc);
    if (!out.good()) return false;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        for (std::size_t c = 0; c < columns.size(); ++c) {
            if (c) out << "\t";
            out << encode_cell(rows[r].values[c]);
        }
        out << "\n";
    }
    return true;
}

bool DatabaseEngine::load_rows(const std::string& table, const MiniVector<ColumnDef>& columns, MiniVector<Row>& rows) const {
    rows.clear();
    std::ifstream in(table_base(table).string() + ".dat");
    if (!in.good()) return false;
    std::string line;
    while (std::getline(in, line)) {
        Row row;
        std::size_t start = 0;
        for (std::size_t c = 0; c < columns.size(); ++c) {
            std::size_t end = line.find('\t', start);
            std::string raw = line.substr(start, end == std::string::npos ? std::string::npos : end - start);
            raw = decode_cell(raw);
            Value v;
            v.type = columns[c].type;
            if (v.type == ColumnType::Int) v.int_value = std::stoi(raw);
            else v.str_value = raw;
            row.values.push_back(v);
            start = end == std::string::npos ? line.size() : end + 1;
        }
        rows.push_back(row);
    }
    return true;
}

bool DatabaseEngine::rebuild_index(const std::string& table, const MiniVector<ColumnDef>& columns, const MiniVector<Row>& rows) {
    int pidx = primary_index(columns);
    std::filesystem::path idx_path = table_base(table).string() + ".idx";
    if (pidx < 0) {
        std::filesystem::remove(idx_path);
        return true;
    }
    BPlusTreeIndex index(columns[pidx].type == ColumnType::String);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!index.insert(rows[i].values[pidx], static_cast<long>(i))) return false;
    }
    return index.save(idx_path);
}

bool DatabaseEngine::row_matches(const Row& row, const MiniVector<ColumnDef>& columns, const Condition& cond) const {
    if (!cond.has) return true;
    int idx = column_index(columns, cond.column);
    if (idx < 0) return false;
    return compare_values(row.values[idx], cond.op, cond.value);
}

QueryResult DatabaseEngine::create_table(const Statement& stmt) {
    QueryResult r;
    if (!require_db(r)) return r;
    if (!is_valid_name(stmt.table)) {
        r.ok = false;
        r.message = "invalid table name, use lowercase letters only";
        return r;
    }
    if (stmt.columns.empty()) {
        r.ok = false;
        r.message = "table must have at least one column";
        return r;
    }
    if (std::filesystem::exists(table_base(stmt.table).string() + ".schema")) {
        r.ok = false;
        r.message = "table already exists";
        return r;
    }
    int primary_count = 0;
    for (std::size_t i = 0; i < stmt.columns.size(); ++i) {
        if (!is_valid_name(stmt.columns[i].name)) {
            r.ok = false;
            r.message = "invalid column name, use lowercase letters only";
            return r;
        }
        if (stmt.columns[i].primary) ++primary_count;
        for (std::size_t j = i + 1; j < stmt.columns.size(); ++j) {
            if (stmt.columns[i].name == stmt.columns[j].name) {
                r.ok = false;
                r.message = "duplicate column name";
                return r;
            }
        }
    }
    if (primary_count > 1) {
        r.ok = false;
        r.message = "only one primary key is supported";
        return r;
    }
    if (!save_schema(stmt.table, stmt.columns)) {
        r.ok = false;
        r.message = "failed to save schema";
        return r;
    }
    std::ofstream(table_base(stmt.table).string() + ".dat", std::ios::trunc).close();
    MiniVector<Row> empty;
    rebuild_index(stmt.table, stmt.columns, empty);
    r.message = "Query OK, table created";
    return r;
}

QueryResult DatabaseEngine::drop_table(const std::string& table) {
    QueryResult r;
    if (!require_db(r)) return r;
    if (!std::filesystem::exists(table_base(table).string() + ".schema")) {
        r.ok = false;
        r.message = "table does not exist";
        return r;
    }
    std::filesystem::remove(table_base(table).string() + ".schema");
    std::filesystem::remove(table_base(table).string() + ".dat");
    std::filesystem::remove(table_base(table).string() + ".idx");
    r.message = "Query OK, table dropped";
    return r;
}

QueryResult DatabaseEngine::insert_row(const Statement& stmt) {
    QueryResult r;
    if (!require_db(r)) return r;
    MiniVector<ColumnDef> columns;
    if (!load_schema(stmt.table, columns)) {
        r.ok = false;
        r.message = "table does not exist";
        return r;
    }
    if (stmt.values.size() != columns.size()) {
        r.ok = false;
        r.message = "column count does not match value count";
        return r;
    }
    Row row;
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (stmt.values[i].type != columns[i].type) {
            r.ok = false;
            r.message = "type mismatch for column " + columns[i].name;
            return r;
        }
        row.values.push_back(stmt.values[i]);
    }

    MiniVector<Row> rows;
    load_rows(stmt.table, columns, rows);
    int pidx = primary_index(columns);
    if (pidx >= 0) {
        BPlusTreeIndex index(columns[pidx].type == ColumnType::String);
        index.load(table_base(stmt.table).string() + ".idx");
        long ignored = 0;
        if (index.find(row.values[pidx], &ignored)) {
            r.ok = false;
            r.message = "duplicate primary key";
            return r;
        }
    }
    rows.push_back(row);
    if (!save_rows(stmt.table, columns, rows) || !rebuild_index(stmt.table, columns, rows)) {
        r.ok = false;
        r.message = "failed to save row";
        return r;
    }
    r.message = "Query OK, 1 row affected";
    return r;
}

QueryResult DatabaseEngine::select_rows(const Statement& stmt) {
    QueryResult r;
    if (!require_db(r)) return r;
    MiniVector<ColumnDef> columns;
    if (!load_schema(stmt.table, columns)) {
        r.ok = false;
        r.message = "table does not exist";
        return r;
    }
    int select_idx = -1;
    if (stmt.select_column != "*") {
        select_idx = column_index(columns, stmt.select_column);
        if (select_idx < 0) {
            r.ok = false;
            r.message = "select column does not exist";
            return r;
        }
    }
    if (stmt.condition.has && column_index(columns, stmt.condition.column) < 0) {
        r.ok = false;
        r.message = "where column does not exist";
        return r;
    }
    MiniVector<Row> rows;
    load_rows(stmt.table, columns, rows);

    if (select_idx >= 0) {
        r.columns.push_back(columns[select_idx].name);
    } else {
        for (std::size_t i = 0; i < columns.size(); ++i) r.columns.push_back(columns[i].name);
    }

    int pidx = primary_index(columns);
    bool used_index = false;
    if (stmt.condition.has && stmt.condition.op == OpType::Eq && pidx >= 0 && columns[pidx].name == stmt.condition.column) {
        BPlusTreeIndex index(columns[pidx].type == ColumnType::String);
        index.load(table_base(stmt.table).string() + ".idx");
        long offset = -1;
        if (index.find(stmt.condition.value, &offset) && offset >= 0 && static_cast<std::size_t>(offset) < rows.size()) {
            Row out;
            if (select_idx >= 0) out.values.push_back(rows[static_cast<std::size_t>(offset)].values[select_idx]);
            else out = rows[static_cast<std::size_t>(offset)];
            r.rows.push_back(out);
        }
        used_index = true;
    }
    if (!used_index) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (!row_matches(rows[i], columns, stmt.condition)) continue;
            Row out;
            if (select_idx >= 0) out.values.push_back(rows[i].values[select_idx]);
            else out = rows[i];
            r.rows.push_back(out);
        }
    }
    r.message = "Query OK";
    return r;
}

QueryResult DatabaseEngine::delete_rows(const Statement& stmt) {
    QueryResult r;
    if (!require_db(r)) return r;
    MiniVector<ColumnDef> columns;
    if (!load_schema(stmt.table, columns)) {
        r.ok = false;
        r.message = "table does not exist";
        return r;
    }
    if (stmt.condition.has && column_index(columns, stmt.condition.column) < 0) {
        r.ok = false;
        r.message = "where column does not exist";
        return r;
    }
    MiniVector<Row> rows;
    MiniVector<Row> kept;
    load_rows(stmt.table, columns, rows);
    int affected = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (row_matches(rows[i], columns, stmt.condition)) ++affected;
        else kept.push_back(rows[i]);
    }
    if (!save_rows(stmt.table, columns, kept) || !rebuild_index(stmt.table, columns, kept)) {
        r.ok = false;
        r.message = "failed to save table";
        return r;
    }
    r.message = "Query OK, " + std::to_string(affected) + " rows affected";
    return r;
}

QueryResult DatabaseEngine::update_rows(const Statement& stmt) {
    QueryResult r;
    if (!require_db(r)) return r;
    MiniVector<ColumnDef> columns;
    if (!load_schema(stmt.table, columns)) {
        r.ok = false;
        r.message = "table does not exist";
        return r;
    }
    int set_idx = column_index(columns, stmt.set_column);
    if (set_idx < 0) {
        r.ok = false;
        r.message = "set column does not exist";
        return r;
    }
    if (stmt.set_value.type != columns[set_idx].type) {
        r.ok = false;
        r.message = "type mismatch for set column";
        return r;
    }
    if (stmt.condition.has && column_index(columns, stmt.condition.column) < 0) {
        r.ok = false;
        r.message = "where column does not exist";
        return r;
    }
    MiniVector<Row> rows;
    load_rows(stmt.table, columns, rows);
    int affected = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (row_matches(rows[i], columns, stmt.condition)) {
            rows[i].values[set_idx] = stmt.set_value;
            ++affected;
        }
    }
    if (!rebuild_index(stmt.table, columns, rows)) {
        r.ok = false;
        r.message = "duplicate primary key after update";
        return r;
    }
    if (!save_rows(stmt.table, columns, rows)) {
        r.ok = false;
        r.message = "failed to save table";
        return r;
    }
    r.message = "Query OK, " + std::to_string(affected) + " rows affected";
    return r;
}

} // namespace minidb

