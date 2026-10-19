// service.h —— 机票业务逻辑
#pragma once

#include <string>
#include <vector>

#include "db.h"

struct Flight {
    int         id = 0;
    std::string flight_no;
    std::string airline;
    std::string origin;
    std::string destination;
    std::string depart_time;
    std::string arrive_time;
    double      price = 0;
    int         total_seats = 0;
    int         seats_left = 0;
};

struct OrderView {
    std::string order_no;
    std::string passenger;
    std::string id_card;
    std::string flight_no;
    std::string route;
    std::string depart_time;
    std::string amount;
    std::string status;     // 已订 / 已退
    std::string created_at;
};

class FlightService {
public:
    explicit FlightService(Db& db) : db_(db) {}

    /** 查询航班；origin/destination/date 传空串表示不过滤 */
    std::vector<Flight> search(const std::string& origin,
                               const std::string& destination,
                               const std::string& date,
                               std::string& err);

    std::vector<Flight> list_all(std::string& err);

    /** 订票：成功时返回订单号，失败时返回空串并写 err */
    std::string book(const std::string& flight_no,
                     const std::string& name,
                     const std::string& id_card,
                     const std::string& phone,
                     std::string& err);

    /** 按身份证号或订单号查订单 */
    std::vector<OrderView> find_orders(const std::string& keyword,
                                       bool only_active,
                                       std::string& err);

    /** 退票：从订单号回补座位 */
    bool cancel(const std::string& order_no, std::string& err);

    /** 新增航班 */
    bool add_flight(const Flight& f, std::string& err);

    /** 运营统计 */
    bool statistics(int& total_orders, int& active_orders,
                    double& revenue, std::string& err);

private:
    Db& db_;

    static std::string now_stamp();
    static std::string make_order_no();
    static std::string fmt_money(double v);
};
