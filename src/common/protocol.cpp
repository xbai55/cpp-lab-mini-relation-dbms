#include "minidb.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace minidb {

bool is_valid_name(const std::string& name) {
    if (name.empty()) return false;
    for (char ch : name) {
        if (ch < 'a' || ch > 'z') return false;
    }
    return true;
}

std::string trim(const std::string& s) {
    std::size_t begin = 0;
    while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin]))) ++begin;
    std::size_t end = s.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(begin, end - begin);
}

std::string lower_copy(const std::string& s) {
    std::string out = s;
    for (char& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return out;
}

std::string value_to_string(const Value& v) {
    if (v.type == ColumnType::Int) return std::to_string(v.int_value);
    return v.str_value;
}

bool compare_values(const Value& left, OpType op, const Value& right) {
    if (left.type != right.type) return false;
    int cmp = 0;
    if (left.type == ColumnType::Int) {
        if (left.int_value < right.int_value) cmp = -1;
        else if (left.int_value > right.int_value) cmp = 1;
    } else {
        cmp = left.str_value.compare(right.str_value);
    }
    if (op == OpType::Eq) return cmp == 0;
    if (op == OpType::Lt) return cmp < 0;
    if (op == OpType::Gt) return cmp > 0;
    return false;
}

static std::string escape_field(const std::string& s) {
    std::string out;
    for (char ch : s) {
        if (ch == '\\' || ch == '|' || ch == '\n') out.push_back('\\');
        if (ch == '\n') out.push_back('n');
        else out.push_back(ch);
    }
    return out;
}

static std::string unescape_field(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            out.push_back(s[i] == 'n' ? '\n' : s[i]);
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

static std::size_t find_unescaped(const std::string& s, char target, std::size_t start = 0) {
    bool escaped = false;
    for (std::size_t i = start; i < s.size(); ++i) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (s[i] == '\\') {
            escaped = true;
            continue;
        }
        if (s[i] == target) return i;
    }
    return std::string::npos;
}

std::string encode_result(const QueryResult& result) {
    std::ostringstream out;
    out << (result.ok ? "OK" : "ERR") << "|" << escape_field(result.message) << "\n";
    out << result.columns.size() << "\n";
    for (std::size_t i = 0; i < result.columns.size(); ++i) out << escape_field(result.columns[i]) << "\n";
    out << result.rows.size() << "\n";
    for (std::size_t r = 0; r < result.rows.size(); ++r) {
        out << result.rows[r].values.size();
        for (std::size_t c = 0; c < result.rows[r].values.size(); ++c) {
            out << "|" << (result.rows[r].values[c].type == ColumnType::Int ? "I:" : "S:");
            out << escape_field(value_to_string(result.rows[r].values[c]));
        }
        out << "\n";
    }
    return out.str();
}

QueryResult decode_result(const std::string& text) {
    QueryResult result;
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line)) {
        result.ok = false;
        result.message = "empty response";
        return result;
    }
    std::size_t bar = find_unescaped(line, '|');
    result.ok = line.substr(0, bar) == "OK";
    result.message = bar == std::string::npos ? "" : unescape_field(line.substr(bar + 1));
    std::getline(in, line);
    int col_count = std::stoi(line);
    for (int i = 0; i < col_count; ++i) {
        std::getline(in, line);
        result.columns.push_back(unescape_field(line));
    }
    std::getline(in, line);
    int row_count = std::stoi(line);
    for (int r = 0; r < row_count; ++r) {
        std::getline(in, line);
        Row row;
        std::size_t start = 0;
        std::size_t sep = find_unescaped(line, '|');
        int value_count = std::stoi(line.substr(0, sep));
        start = sep == std::string::npos ? line.size() : sep + 1;
        for (int c = 0; c < value_count; ++c) {
            sep = find_unescaped(line, '|', start);
            std::string part = line.substr(start, sep == std::string::npos ? std::string::npos : sep - start);
            Value v;
            v.type = part.rfind("I:", 0) == 0 ? ColumnType::Int : ColumnType::String;
            std::string raw = unescape_field(part.substr(2));
            if (v.type == ColumnType::Int) v.int_value = std::stoi(raw);
            else v.str_value = raw;
            row.values.push_back(v);
            start = sep == std::string::npos ? line.size() : sep + 1;
        }
        result.rows.push_back(row);
    }
    return result;
}

std::string format_result_table(const QueryResult& result) {
    if (!result.ok) return "ERROR: " + result.message + "\n";
    if (result.columns.empty()) return result.message.empty() ? "Query OK\n" : result.message + "\n";

    MiniVector<std::size_t> widths;
    for (std::size_t c = 0; c < result.columns.size(); ++c) widths.push_back(result.columns[c].size());
    for (std::size_t r = 0; r < result.rows.size(); ++r) {
        for (std::size_t c = 0; c < result.rows[r].values.size(); ++c) {
            std::size_t len = value_to_string(result.rows[r].values[c]).size();
            if (len > widths[c]) widths[c] = len;
        }
    }

    auto border = [&]() {
        std::string s = "+";
        for (std::size_t c = 0; c < widths.size(); ++c) s += std::string(widths[c] + 2, '-') + "+";
        return s + "\n";
    };

    std::ostringstream out;
    out << border() << "|";
    for (std::size_t c = 0; c < result.columns.size(); ++c) {
        out << " " << result.columns[c] << std::string(widths[c] - result.columns[c].size(), ' ') << " |";
    }
    out << "\n" << border();
    for (std::size_t r = 0; r < result.rows.size(); ++r) {
        out << "|";
        for (std::size_t c = 0; c < result.rows[r].values.size(); ++c) {
            std::string cell = value_to_string(result.rows[r].values[c]);
            out << " " << cell << std::string(widths[c] - cell.size(), ' ') << " |";
        }
        out << "\n";
    }
    out << border() << result.rows.size() << (result.rows.size() == 1 ? " row" : " rows") << " in set\n";
    return out.str();
}

static bool write_all(SocketHandle fd, const char* data, std::size_t len) {
    std::size_t sent = 0;
    while (sent < len) {
#ifdef _WIN32
        int n = ::send(static_cast<SOCKET>(fd), data + sent, static_cast<int>(len - sent), 0);
#else
        ssize_t n = ::send(fd, data + sent, len - sent, 0);
#endif
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

static bool read_all(SocketHandle fd, char* data, std::size_t len) {
    std::size_t got = 0;
    while (got < len) {
#ifdef _WIN32
        int n = ::recv(static_cast<SOCKET>(fd), data + got, static_cast<int>(len - got), 0);
#else
        ssize_t n = ::recv(fd, data + got, len - got, 0);
#endif
        if (n <= 0) return false;
        got += static_cast<std::size_t>(n);
    }
    return true;
}

bool send_message(SocketHandle fd, const std::string& payload) {
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    return write_all(fd, reinterpret_cast<const char*>(&len), sizeof(len)) &&
           write_all(fd, payload.data(), payload.size());
}

bool recv_message(SocketHandle fd, std::string& payload) {
    uint32_t len_net = 0;
    if (!read_all(fd, reinterpret_cast<char*>(&len_net), sizeof(len_net))) return false;
    uint32_t len = ntohl(len_net);
    payload.assign(len, '\0');
    return len == 0 || read_all(fd, payload.data(), len);
}

} // namespace minidb
