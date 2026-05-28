#include "minidb.hpp"

namespace minidb {

void Parser::tokenize(const std::string& sql) {
    tokens_.clear();
    pos_ = 0;
    for (std::size_t i = 0; i < sql.size();) {
        unsigned char ch = static_cast<unsigned char>(sql[i]);
        if (std::isspace(ch)) {
            ++i;
        } else if (std::isalpha(ch)) {
            std::size_t begin = i;
            while (i < sql.size() && std::isalpha(static_cast<unsigned char>(sql[i]))) ++i;
            tokens_.push_back(sql.substr(begin, i - begin));
        } else if (std::isdigit(ch) || (sql[i] == '-' && i + 1 < sql.size() && std::isdigit(static_cast<unsigned char>(sql[i + 1])))) {
            std::size_t begin = i++;
            while (i < sql.size() && std::isdigit(static_cast<unsigned char>(sql[i]))) ++i;
            tokens_.push_back(sql.substr(begin, i - begin));
        } else if (sql[i] == '"') {
            ++i;
            std::string s;
            while (i < sql.size() && sql[i] != '"') {
                if (sql[i] == '\\' && i + 1 < sql.size()) ++i;
                s.push_back(sql[i++]);
            }
            if (i >= sql.size()) throw std::runtime_error("SQL syntax error: missing closing quote");
            ++i;
            tokens_.push_back("\"" + s + "\"");
        } else {
            tokens_.push_back(sql.substr(i, 1));
            ++i;
        }
    }
}

bool Parser::accept(const std::string& token) {
    if (!end() && peek() == token) {
        ++pos_;
        return true;
    }
    return false;
}

std::string Parser::expect_word(const std::string& what) {
    if (end()) throw std::runtime_error("SQL syntax error: expected " + what);
    std::string word = next();
    if (word.empty() || !std::isalpha(static_cast<unsigned char>(word[0]))) {
        throw std::runtime_error("SQL syntax error: expected " + what);
    }
    return word;
}

void Parser::expect(const std::string& token) {
    if (!accept(token)) throw std::runtime_error("SQL syntax error: expected '" + token + "'");
}

bool Parser::end() const { return pos_ >= tokens_.size(); }
std::string Parser::peek() const { return end() ? "" : tokens_[pos_]; }
std::string Parser::next() { return end() ? "" : tokens_[pos_++]; }

Value Parser::parse_value() {
    if (end()) throw std::runtime_error("SQL syntax error: expected value");
    std::string raw = next();
    Value v;
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
        v.type = ColumnType::String;
        v.str_value = raw.substr(1, raw.size() - 2);
        if (v.str_value.size() > 256) throw std::runtime_error("string value is longer than 256 bytes");
    } else if (!raw.empty() && (std::isdigit(static_cast<unsigned char>(raw[0])) || raw[0] == '-')) {
        v.type = ColumnType::Int;
        try {
            v.int_value = std::stoi(raw);
        } catch (...) {
            throw std::runtime_error("SQL syntax error: invalid value");
        }
    } else if (!raw.empty() && std::isalpha(static_cast<unsigned char>(raw[0]))) {
        v.type = ColumnType::String;
        v.str_value = raw;
        if (v.str_value.size() > 256) throw std::runtime_error("string value is longer than 256 bytes");
    } else {
        throw std::runtime_error("SQL syntax error: invalid value");
    }
    return v;
}

Condition Parser::parse_optional_where() {
    Condition cond;
    if (!accept("where")) return cond;
    cond.has = true;
    cond.column = expect_word("where column");
    std::string op = next();
    if (op == "=") cond.op = OpType::Eq;
    else if (op == "<") cond.op = OpType::Lt;
    else if (op == ">") cond.op = OpType::Gt;
    else throw std::runtime_error("SQL syntax error: expected operator =, < or >");
    cond.value = parse_value();
    return cond;
}

Statement Parser::parse(const std::string& sql) {
    tokenize(sql);
    Statement stmt;
    if (tokens_.empty()) throw std::runtime_error("SQL syntax error: empty input");

    std::string first = next();
    if (first == "exit") {
        stmt.type = StatementType::Exit;
    } else if (first == "create") {
        if (accept("database")) {
            stmt.type = StatementType::CreateDatabase;
            stmt.database = expect_word("database name");
        } else if (accept("table")) {
            stmt.type = StatementType::CreateTable;
            stmt.table = expect_word("table name");
            expect("(");
            while (true) {
                ColumnDef col;
                col.name = expect_word("column name");
                std::string type = next();
                if (type == "int") col.type = ColumnType::Int;
                else if (type == "string") col.type = ColumnType::String;
                else throw std::runtime_error("SQL syntax error: unknown column type");
                if (accept("primary")) col.primary = true;
                stmt.columns.push_back(col);
                if (accept(")")) break;
                expect(",");
            }
        } else {
            throw std::runtime_error("SQL syntax error: expected database or table");
        }
    } else if (first == "drop") {
        if (accept("database")) {
            stmt.type = StatementType::DropDatabase;
            stmt.database = expect_word("database name");
        } else if (accept("table")) {
            stmt.type = StatementType::DropTable;
            stmt.table = expect_word("table name");
        } else {
            throw std::runtime_error("SQL syntax error: expected database or table");
        }
    } else if (first == "use") {
        stmt.type = StatementType::UseDatabase;
        stmt.database = expect_word("database name");
    } else if (first == "insert") {
        stmt.type = StatementType::Insert;
        stmt.table = expect_word("table name");
        expect("values");
        expect("(");
        while (true) {
            stmt.values.push_back(parse_value());
            if (accept(")")) break;
            expect(",");
        }
    } else if (first == "select") {
        stmt.type = StatementType::Select;
        stmt.select_column = next();
        expect("from");
        stmt.table = expect_word("table name");
        stmt.condition = parse_optional_where();
    } else if (first == "delete") {
        stmt.type = StatementType::DeleteRows;
        stmt.table = expect_word("table name");
        stmt.condition = parse_optional_where();
    } else if (first == "update") {
        stmt.type = StatementType::Update;
        stmt.table = expect_word("table name");
        expect("set");
        stmt.set_column = expect_word("column name");
        expect("=");
        stmt.set_value = parse_value();
        stmt.condition = parse_optional_where();
    } else {
        throw std::runtime_error("SQL syntax error: unknown command");
    }

    if (!end()) throw std::runtime_error("SQL syntax error: unexpected token '" + peek() + "'");
    return stmt;
}

} // namespace minidb
