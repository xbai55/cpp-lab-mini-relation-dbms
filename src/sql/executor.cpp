#include "minidb.hpp"

namespace minidb {

QueryResult DatabaseEngine::execute_sql(const std::string& sql) {
    try {
        Parser parser;
        Statement stmt = parser.parse(sql);
        return execute(stmt);
    } catch (const std::exception& e) {
        QueryResult r;
        r.ok = false;
        r.message = e.what();
        return r;
    }
}

QueryResult DatabaseEngine::execute(const Statement& stmt) {
    switch (stmt.type) {
        case StatementType::CreateDatabase: return create_database(stmt.database);
        case StatementType::DropDatabase: return drop_database(stmt.database);
        case StatementType::UseDatabase: return use_database(stmt.database);
        case StatementType::CreateTable: return create_table(stmt);
        case StatementType::DropTable: return drop_table(stmt.table);
        case StatementType::Insert: return insert_row(stmt);
        case StatementType::Select: return select_rows(stmt);
        case StatementType::DeleteRows: return delete_rows(stmt);
        case StatementType::Update: return update_rows(stmt);
        case StatementType::Exit: {
            QueryResult r;
            r.message = "Bye";
            return r;
        }
        default: {
            QueryResult r;
            r.ok = false;
            r.message = "invalid statement";
            return r;
        }
    }
}

} // namespace minidb

