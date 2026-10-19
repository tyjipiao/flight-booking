// db.h —— MySQL 连接的极简 RAII 封装
#pragma once

#include <mysql.h>

#include <cstdint>
#include <stdexcept>
#include <string>

struct ConnConfig {
    std::string host = "127.0.0.1";
    unsigned    port = 3306;
    std::string user;
    std::string password;
    std::string database;
    unsigned    timeout = 5;  // 连接超时（秒）
};

class Db {
public:
    explicit Db(const ConnConfig& cfg) {
        conn_ = mysql_init(nullptr);
        if (!conn_) throw std::runtime_error("mysql_init 失败");

        unsigned timeout = cfg.timeout;
        mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4");

        if (!mysql_real_connect(conn_, cfg.host.c_str(), cfg.user.c_str(),
                                cfg.password.c_str(), cfg.database.c_str(),
                                cfg.port, nullptr, 0)) {
            std::string msg = mysql_error(conn_);
            mysql_close(conn_);
            conn_ = nullptr;
            throw std::runtime_error("连接数据库失败：" + msg);
        }
    }

    ~Db() {
        if (conn_) mysql_close(conn_);
    }

    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;

    /** 执行不关心结果集的语句，成功返回 true */
    bool exec(const std::string& sql, std::string& err) {
        if (mysql_real_query(conn_, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
            err = mysql_error(conn_);
            return false;
        }
        return true;
    }

    /** 执行返回结果集的语句；失败返回 nullptr。用完须 mysql_free_result */
    MYSQL_RES* query(const std::string& sql, std::string& err) {
        if (mysql_real_query(conn_, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
            err = mysql_error(conn_);
            return nullptr;
        }
        return mysql_store_result(conn_);
    }

    std::uint64_t affected_rows() const { return mysql_affected_rows(conn_); }
    std::uint64_t last_insert_id() const { return mysql_insert_id(conn_); }

    /** 转义后返回可直接拼进 SQL 单引号内的字符串 */
    std::string escape(const std::string& s) const {
        std::string out(s.size() * 2 + 1, '\0');
        unsigned long n = mysql_real_escape_string(
            conn_, out.data(), s.c_str(), static_cast<unsigned long>(s.size()));
        out.resize(n);
        return out;
    }

    /** 事务便捷封装 */
    bool begin(std::string& err) { return exec("START TRANSACTION", err); }
    bool commit(std::string& err) { return exec("COMMIT", err); }
    bool rollback() {
        std::string e;
        return exec("ROLLBACK", e);
    }

    MYSQL* raw() { return conn_; }

private:
    MYSQL* conn_ = nullptr;
};
