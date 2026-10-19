// main.cpp —— 机票预订系统（控制台版）
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "db.h"
#include "service.h"

namespace {

// ---------------- 配置 ----------------
struct AppConfig {
    ConnConfig  conn;
    std::string admin_password = "admin123";
};

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::string exe_dir() {
    char buf[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf);
    size_t pos = p.find_last_of("\\/");
    return pos == std::string::npos ? "." : p.substr(0, pos);
}

/** 依次尝试：exe 同目录、上一级、当前目录、项目根（bin/../） */
bool load_config(AppConfig& cfg, std::string& err) {
    std::vector<std::string> candidates = {
        exe_dir() + "\\config.ini",
        exe_dir() + "\\..\\config.ini",
        "config.ini",
        "..\\config.ini",
    };

    std::ifstream in;
    std::string used;
    for (const auto& path : candidates) {
        in.open(path);
        if (in.is_open()) { used = path; break; }
    }
    if (!in.is_open()) {
        err = "找不到 config.ini（应放在程序目录或项目根目录）";
        return false;
    }

    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (section == "mysql") {
            if (key == "host")     cfg.conn.host = val;
            else if (key == "port")     cfg.conn.port = static_cast<unsigned>(std::atoi(val.c_str()));
            else if (key == "user")     cfg.conn.user = val;
            else if (key == "password") cfg.conn.password = val;
            else if (key == "database") cfg.conn.database = val;
        } else if (section == "admin") {
            if (key == "password") cfg.admin_password = val;
        }
    }
    if (cfg.conn.user.empty() || cfg.conn.database.empty()) {
        err = "config.ini 缺少 [mysql] user 或 database";
        return false;
    }
    err = used;
    return true;
}

// ---------------- 显示宽度（中文占 2 列） ----------------
int char_width(unsigned char c) {
    // UTF-8 首字节判断：0xE0 以上按双宽处理（覆盖绝大部分中文/全角字符）
    return (c >= 0xE0) ? 2 : 1;
}

int display_width(const std::string& s) {
    int w = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) == 0x80) continue;  // 续字节跳过
        w += char_width(c);
    }
    return w;
}

std::string pad(const std::string& s, int width) {
    std::string out = s;
    int w = display_width(s);
    if (w < width) out.append(static_cast<size_t>(width - w), ' ');
    return out;
}

std::string line(char c = '-', int n = 96) { return std::string(n, c); }

void print_header(const std::string& title) {
    std::cout << "\n" << line('=') << "\n  " << title << "\n" << line('=') << "\n";
}

void print_flights(const std::vector<Flight>& list) {
    if (list.empty()) {
        std::cout << "  没有符合条件的航班。\n";
        return;
    }
    std::cout << pad("航班号", 9) << pad("航空公司", 11) << pad("航线", 18)
              << pad("起飞时间", 19) << pad("到达时间", 19)
              << pad("票价", 10) << "余票\n";
    std::cout << line() << "\n";
    for (const auto& f : list) {
        std::string route = f.origin + " → " + f.destination;
        char price[32];
        std::snprintf(price, sizeof(price), "%.2f", f.price);
        std::cout << pad(f.flight_no, 9) << pad(f.airline, 11) << pad(route, 18)
                  << pad(f.depart_time, 19) << pad(f.arrive_time, 19)
                  << pad(price, 10) << f.seats_left << "\n";
    }
    std::cout << line() << "\n共 " << list.size() << " 条航班\n";
}

void print_orders(const std::vector<OrderView>& list) {
    if (list.empty()) {
        std::cout << "  没有找到订单。\n";
        return;
    }
    std::cout << pad("订单号", 20) << pad("乘客", 10) << pad("航班", 9)
              << pad("航线", 18) << pad("起飞时间", 19) << pad("金额", 10)
              << pad("状态", 8) << "下单时间\n";
    std::cout << line(('-'), 110) << "\n";
    for (const auto& o : list) {
        std::cout << pad(o.order_no, 20) << pad(o.passenger, 10) << pad(o.flight_no, 9)
                  << pad(o.route, 18) << pad(o.depart_time, 19) << pad(o.amount, 10)
                  << pad(o.status, 8) << o.created_at << "\n";
    }
    std::cout << line('-', 110) << "\n共 " << list.size() << " 条订单\n";
}

std::string ask(const std::string& prompt) {
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    return trim(s);
}

// ---------------- 各功能 ----------------
void menu_search(FlightService& svc) {
    print_header("查询航班");
    std::string origin = ask("出发城市（回车跳过）：");
    std::string dest   = ask("到达城市（回车跳过）：");
    std::string date   = ask("出发日期 YYYY-MM-DD（回车跳过）：");

    std::string err;
    auto list = svc.search(origin, dest, date, err);
    if (!err.empty()) { std::cout << "查询失败：" << err << "\n"; return; }
    print_flights(list);
}

void menu_book(FlightService& svc) {
    print_header("预订机票");
    std::string err;
    auto list = svc.search("", "", "", err);
    if (!err.empty()) { std::cout << "读取航班失败：" << err << "\n"; return; }
    print_flights(list);

    std::string flight_no = ask("请输入要预订的航班号（回车取消）：");
    if (flight_no.empty()) return;

    std::string name    = ask("乘客姓名：");
    std::string id_card = ask("身份证号（18 位）：");
    std::string phone   = ask("手机号（可空）：");
    if (name.empty() || id_card.size() != 18) {
        std::cout << "姓名或身份证号不合法，已取消。\n";
        return;
    }

    std::string order_no = svc.book(flight_no, name, id_card, phone, err);
    if (order_no.empty()) {
        std::cout << "订票失败：" << err << "\n";
        return;
    }
    std::cout << "\n✅ 订票成功！订单号：" << order_no << "\n";
    std::cout << "   请凭订单号或身份证号查询/退票。\n";
}

void menu_find_orders(FlightService& svc) {
    print_header("查询订单");
    std::string keyword = ask("输入订单号或身份证号（回车查看最近全部）：");
    std::string err;
    auto list = svc.find_orders(keyword, false, err);
    if (!err.empty()) { std::cout << "查询失败：" << err << "\n"; return; }
    print_orders(list);
}

void menu_cancel(FlightService& svc) {
    print_header("退票");
    std::string order_no = ask("请输入订单号：");
    if (order_no.empty()) return;
    std::string confirm = ask("确认退票？(y/N)：");
    if (confirm != "y" && confirm != "Y") { std::cout << "已取消操作。\n"; return; }

    std::string err;
    if (svc.cancel(order_no, err)) {
        std::cout << "✅ 退票成功，座位已释放。\n";
    } else {
        std::cout << "退票失败：" << err << "\n";
    }
}

void menu_admin_add_flight(FlightService& svc) {
    print_header("新增航班");
    Flight f;
    f.flight_no   = ask("航班号（如 GY2001）：");
    f.airline     = ask("航空公司：");
    f.origin      = ask("出发城市：");
    f.destination = ask("到达城市：");
    std::string depart = ask("起飞时间 YYYY-MM-DD HH:MM: ");
    std::string arrive = ask("到达时间 YYYY-MM-DD HH:MM: ");
    std::string price  = ask("票价（元）：");
    std::string seats  = ask("总座位数：");
    if (f.flight_no.empty() || depart.empty() || arrive.empty()) {
        std::cout << "信息不完整，已取消。\n";
        return;
    }
    f.depart_time = depart + ":00";
    f.arrive_time = arrive + ":00";
    f.price       = std::atof(price.c_str());
    f.total_seats = std::atoi(seats.c_str());
    if (f.total_seats <= 0) f.total_seats = 100;

    std::string err;
    if (svc.add_flight(f, err)) {
        std::cout << "✅ 航班 " << f.flight_no << " 已添加。\n";
    } else {
        std::cout << "添加失败：" << err << "\n";
    }
}

void menu_admin(FlightService& svc, const AppConfig& cfg) {
    std::string pwd = ask("管理员口令：");
    if (pwd != cfg.admin_password) {
        std::cout << "口令错误。\n";
        return;
    }
    for (;;) {
        print_header("管理后台");
        std::cout << "  1) 新增航班\n"
                  << "  2) 查看全部订单\n"
                  << "  3) 运营统计\n"
                  << "  0) 返回主菜单\n";
        std::string choice = ask("请选择：");
        if (choice == "0") return;
        if (choice == "1") {
            menu_admin_add_flight(svc);
        } else if (choice == "2") {
            std::string err;
            auto list = svc.find_orders("", false, err);
            if (!err.empty()) std::cout << "查询失败：" << err << "\n";
            else print_orders(list);
        } else if (choice == "3") {
            int total = 0, active = 0;
            double revenue = 0;
            std::string err;
            if (svc.statistics(total, active, revenue, err)) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.2f", revenue);
                std::cout << "  订单总数：" << total << "\n"
                          << "  有效订单：" << active << "\n"
                          << "  累计票款：" << buf << " 元\n";
            } else {
                std::cout << "统计失败：" << err << "\n";
            }
        }
    }
}

}  // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    AppConfig cfg;
    std::string cfg_path;
    if (!load_config(cfg, cfg_path)) {
        std::cout << "启动失败：" << cfg_path << "\n"
                  << "请把 config.ini 放在程序目录或项目根目录。\n";
        system("pause");
        return 1;
    }

    try {
        Db db(cfg.conn);
        FlightService svc(db);

        for (;;) {
            print_header("机票预订系统");
            std::cout << "  1) 查询航班\n"
                      << "  2) 预订机票\n"
                      << "  3) 查询订单\n"
                      << "  4) 退票\n"
                      << "  5) 管理后台\n"
                      << "  0) 退出\n";
            std::string choice = ask("请选择：");
            if (choice == "0") break;
            if (choice == "1")      menu_search(svc);
            else if (choice == "2") menu_book(svc);
            else if (choice == "3") menu_find_orders(svc);
            else if (choice == "4") menu_cancel(svc);
            else if (choice == "5") menu_admin(svc, cfg);
            else std::cout << "无效选项。\n";
        }
        std::cout << "再见。\n";
    } catch (const std::exception& ex) {
        std::cout << "\n❌ " << ex.what() << "\n"
                  << "请检查：MySQL 服务是否启动、config.ini 账号密码是否正确、"
                     "是否已执行 sql/schema.sql。\n";
        system("pause");
        return 1;
    }
    return 0;
}
