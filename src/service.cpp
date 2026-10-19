// service.cpp
#include "service.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>

namespace {

std::string col(MYSQL_ROW row, unsigned idx) {
    return row[idx] ? row[idx] : "";
}

double col_double(MYSQL_ROW row, unsigned idx) {
    return row[idx] ? std::atof(row[idx]) : 0.0;
}

}  // namespace

std::string FlightService::now_stamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d%02d%02d%02d%02d%02d",
                  tm.tm_year % 100, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

std::string FlightService::make_order_no() {
    std::ostringstream os;
    os << "FB" << now_stamp() << (std::rand() % 900 + 100);
    return os.str();
}

std::string FlightService::fmt_money(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

std::vector<Flight> FlightService::search(const std::string& origin,
                                          const std::string& destination,
                                          const std::string& date,
                                          std::string& err) {
    std::vector<Flight> out;
    std::string sql =
        "SELECT id, flight_no, airline, origin, destination, "
        "DATE_FORMAT(depart_time,'%Y-%m-%d %H:%i'), "
        "DATE_FORMAT(arrive_time,'%Y-%m-%d %H:%i'), "
        "price, total_seats, seats_left FROM flights WHERE status = 1";

    if (!origin.empty())      sql += " AND origin = '" + db_.escape(origin) + "'";
    if (!destination.empty()) sql += " AND destination = '" + db_.escape(destination) + "'";
    if (!date.empty())        sql += " AND DATE(depart_time) = '" + db_.escape(date) + "'";
    sql += " ORDER BY depart_time LIMIT 100";

    MYSQL_RES* res = db_.query(sql, err);
    if (!res) return out;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        Flight f;
        f.id          = std::atoi(col(row, 0).c_str());
        f.flight_no   = col(row, 1);
        f.airline     = col(row, 2);
        f.origin      = col(row, 3);
        f.destination = col(row, 4);
        f.depart_time = col(row, 5);
        f.arrive_time = col(row, 6);
        f.price       = col_double(row, 7);
        f.total_seats = std::atoi(col(row, 8).c_str());
        f.seats_left  = std::atoi(col(row, 9).c_str());
        out.push_back(f);
    }
    mysql_free_result(res);
    return out;
}

std::vector<Flight> FlightService::list_all(std::string& err) {
    return search("", "", "", err);
}

std::string FlightService::book(const std::string& flight_no,
                                const std::string& name,
                                const std::string& id_card,
                                const std::string& phone,
                                std::string& err) {
    if (!db_.begin(err)) return "";

    auto fail = [&](const std::string& msg) -> std::string {
        db_.rollback();
        err = msg;
        return "";
    };

    // 1) 锁住航班行，避免并发超卖
    std::string sql =
        "SELECT id, price, seats_left FROM flights WHERE flight_no = '" +
        db_.escape(flight_no) + "' AND status = 1 FOR UPDATE";
    MYSQL_RES* res = db_.query(sql, err);
    if (!res) { db_.rollback(); return ""; }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return fail("没有找到航班 " + flight_no);
    }
    int    flight_id  = std::atoi(col(row, 0).c_str());
    double price      = col_double(row, 1);
    int    seats_left = std::atoi(col(row, 2).c_str());
    mysql_free_result(res);

    if (seats_left <= 0) return fail("该航班已售罄");

    // 2) 扣座
    char upd[256];
    std::snprintf(upd, sizeof(upd),
                  "UPDATE flights SET seats_left = seats_left - 1 WHERE id = %d",
                  flight_id);
    if (!db_.exec(upd, err)) return fail(err);

    // 3) 乘客信息：按身份证去重，存在则更新姓名/手机
    std::string pname = db_.escape(name);
    std::string pid   = db_.escape(id_card);
    std::string pph   = db_.escape(phone);
    std::string upsert =
        "INSERT INTO passengers (name, id_card, phone) VALUES ('" + pname + "','" +
        pid + "','" + pph + "') ON DUPLICATE KEY UPDATE name = VALUES(name), "
        "phone = VALUES(phone)";
    if (!db_.exec(upsert, err)) return fail(err);

    std::string sel = "SELECT id FROM passengers WHERE id_card = '" + pid + "'";
    res = db_.query(sel, err);
    if (!res) { db_.rollback(); return ""; }
    row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return fail("保存乘客信息失败");
    }
    int passenger_id = std::atoi(col(row, 0).c_str());
    mysql_free_result(res);

    // 4) 生成订单（订单号重复时重试几次）
    std::string order_no;
    for (int attempt = 0; attempt < 5; ++attempt) {
        order_no = make_order_no();
        char ins[512];
        std::snprintf(ins, sizeof(ins),
                      "INSERT INTO orders (order_no, flight_id, passenger_id, amount) "
                      "VALUES ('%s', %d, %d, %.2f)",
                      order_no.c_str(), flight_id, passenger_id, price);
        if (db_.exec(ins, err)) {
            if (!db_.commit(err)) return "";
            return order_no;
        }
        // 1062 = 唯一键冲突，换一个号重试
        if (err.find("Duplicate entry") == std::string::npos) return fail(err);
    }
    return fail("订单号生成冲突，请重试");
}

std::vector<OrderView> FlightService::find_orders(const std::string& keyword,
                                                  bool only_active,
                                                  std::string& err) {
    std::vector<OrderView> out;
    std::string sql =
        "SELECT o.order_no, p.name, p.id_card, f.flight_no, "
        "CONCAT(f.origin,' → ',f.destination), "
        "DATE_FORMAT(f.depart_time,'%Y-%m-%d %H:%i'), "
        "o.amount, o.status, DATE_FORMAT(o.created_at,'%Y-%m-%d %H:%i') "
        "FROM orders o "
        "JOIN flights f ON f.id = o.flight_id "
        "JOIN passengers p ON p.id = o.passenger_id ";

    std::string where;
    if (!keyword.empty()) {
        std::string k = db_.escape(keyword);
        where = " WHERE o.order_no = '" + k + "' OR p.id_card = '" + k + "'";
    }
    if (only_active) where += (where.empty() ? " WHERE " : " AND ") + std::string("o.status = 1");
    sql += where + " ORDER BY o.created_at DESC LIMIT 100";

    MYSQL_RES* res = db_.query(sql, err);
    if (!res) return out;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        OrderView v;
        v.order_no    = col(row, 0);
        v.passenger   = col(row, 1);
        v.id_card     = col(row, 2);
        v.flight_no   = col(row, 3);
        v.route       = col(row, 4);
        v.depart_time = col(row, 5);
        v.amount      = fmt_money(col_double(row, 6));
        v.status      = std::string(col(row, 7)) == "1" ? "已订" : "已退";
        v.created_at  = col(row, 8);
        out.push_back(v);
    }
    mysql_free_result(res);
    return out;
}

bool FlightService::cancel(const std::string& order_no, std::string& err) {
    if (!db_.begin(err)) return false;

    auto fail = [&](const std::string& msg) {
        db_.rollback();
        err = msg;
        return false;
    };

    std::string sql =
        "SELECT id, flight_id, status FROM orders WHERE order_no = '" +
        db_.escape(order_no) + "' FOR UPDATE";
    MYSQL_RES* res = db_.query(sql, err);
    if (!res) { db_.rollback(); return false; }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return fail("没有找到订单 " + order_no);
    }
    int order_id  = std::atoi(col(row, 0).c_str());
    int flight_id = std::atoi(col(row, 1).c_str());
    int status    = std::atoi(col(row, 2).c_str());
    mysql_free_result(res);

    if (status != 1) return fail("该订单已经退票，无需重复操作");

    char upd_order[256];
    std::snprintf(upd_order, sizeof(upd_order),
                  "UPDATE orders SET status = 0, canceled_at = NOW() WHERE id = %d",
                  order_id);
    if (!db_.exec(upd_order, err)) return fail(err);

    char upd_flight[256];
    std::snprintf(upd_flight, sizeof(upd_flight),
                  "UPDATE flights SET seats_left = seats_left + 1 WHERE id = %d",
                  flight_id);
    if (!db_.exec(upd_flight, err)) return fail(err);

    return db_.commit(err);
}

bool FlightService::add_flight(const Flight& f, std::string& err) {
    std::string sql =
        "INSERT INTO flights (flight_no, airline, origin, destination, depart_time, "
        "arrive_time, price, total_seats, seats_left) VALUES ('" +
        db_.escape(f.flight_no) + "','" + db_.escape(f.airline) + "','" +
        db_.escape(f.origin) + "','" + db_.escape(f.destination) + "','" +
        db_.escape(f.depart_time) + "','" + db_.escape(f.arrive_time) + "'," +
        fmt_money(f.price) + "," + std::to_string(f.total_seats) + "," +
        std::to_string(f.total_seats) + ")";
    return db_.exec(sql, err);
}

bool FlightService::statistics(int& total_orders, int& active_orders,
                               double& revenue, std::string& err) {
    std::string sql =
        "SELECT COUNT(*), SUM(status = 1), IFNULL(SUM(CASE WHEN status = 1 THEN amount ELSE 0 END), 0) "
        "FROM orders";
    MYSQL_RES* res = db_.query(sql, err);
    if (!res) return false;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        total_orders  = std::atoi(col(row, 0).c_str());
        active_orders = std::atoi(col(row, 1).c_str());
        revenue       = col_double(row, 2);
    }
    mysql_free_result(res);
    return true;
}
