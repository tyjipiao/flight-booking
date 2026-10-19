-- ============================================================
--  机票预订系统 · 数据库初始化脚本
--  适用：MySQL 8.0（本机 3306）
--  执行方式：在 MySQL Workbench 里打开本文件，用 root 账号运行
--            （Query 菜单 → Execute All，或点闪电图标）
--  说明：脚本可重复执行，会重建表并写入示例航班
-- ============================================================

CREATE DATABASE IF NOT EXISTS flight_booking
  DEFAULT CHARACTER SET utf8mb4
  COLLATE utf8mb4_general_ci;

USE flight_booking;

-- 外键依赖，先删子表
DROP TABLE IF EXISTS orders;
DROP TABLE IF EXISTS flights;
DROP TABLE IF EXISTS passengers;

-- ------------------------------------------------------------
-- 航班表
-- ------------------------------------------------------------
CREATE TABLE flights (
  id           INT AUTO_INCREMENT PRIMARY KEY,
  flight_no    VARCHAR(10)  NOT NULL COMMENT '航班号，如 GY1001',
  airline      VARCHAR(30)  NOT NULL COMMENT '航空公司',
  origin       VARCHAR(30)  NOT NULL COMMENT '出发城市',
  destination  VARCHAR(30)  NOT NULL COMMENT '到达城市',
  depart_time  DATETIME     NOT NULL COMMENT '起飞时间',
  arrive_time  DATETIME     NOT NULL COMMENT '到达时间',
  price        DECIMAL(10,2) NOT NULL COMMENT '票价（元）',
  total_seats  INT          NOT NULL DEFAULT 100 COMMENT '总座位数',
  seats_left   INT          NOT NULL DEFAULT 100 COMMENT '剩余座位',
  status       TINYINT      NOT NULL DEFAULT 1 COMMENT '1正常 0取消',
  UNIQUE KEY uk_flight_no (flight_no),
  KEY idx_route_time (origin, destination, depart_time)
) ENGINE=InnoDB COMMENT='航班';

-- ------------------------------------------------------------
-- 乘客表（身份证唯一，重复订票复用同一条记录）
-- ------------------------------------------------------------
CREATE TABLE passengers (
  id        INT AUTO_INCREMENT PRIMARY KEY,
  name      VARCHAR(30) NOT NULL COMMENT '姓名',
  id_card   CHAR(18)    NOT NULL COMMENT '身份证号',
  phone     VARCHAR(20) DEFAULT NULL COMMENT '手机号',
  UNIQUE KEY uk_id_card (id_card)
) ENGINE=InnoDB COMMENT='乘客';

-- ------------------------------------------------------------
-- 订单表
-- ------------------------------------------------------------
CREATE TABLE orders (
  id           INT AUTO_INCREMENT PRIMARY KEY,
  order_no     CHAR(20)     NOT NULL COMMENT '订单号',
  flight_id    INT          NOT NULL,
  passenger_id INT          NOT NULL,
  amount       DECIMAL(10,2) NOT NULL COMMENT '订单金额',
  status       TINYINT      NOT NULL DEFAULT 1 COMMENT '1已订 0已退',
  created_at   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '下单时间',
  canceled_at  DATETIME     DEFAULT NULL COMMENT '退票时间',
  UNIQUE KEY uk_order_no (order_no),
  KEY idx_passenger (passenger_id),
  KEY idx_flight (flight_id),
  CONSTRAINT fk_order_flight    FOREIGN KEY (flight_id)    REFERENCES flights(id),
  CONSTRAINT fk_order_passenger FOREIGN KEY (passenger_id) REFERENCES passengers(id)
) ENGINE=InnoDB COMMENT='订单';

-- ------------------------------------------------------------
-- 示例航班（日期取 2026 年 11 月，便于演示）
-- ------------------------------------------------------------
INSERT INTO flights (flight_no, airline, origin, destination, depart_time, arrive_time, price, total_seats, seats_left) VALUES
('GY1001', '南方航空', '贵阳', '北京', '2026-11-05 08:30:00', '2026-11-05 11:40:00', 1280.00, 180, 180),
('GY1002', '中国国航', '贵阳', '上海', '2026-11-05 10:15:00', '2026-11-05 12:50:00', 1150.00, 160, 160),
('GY1003', '东方航空', '贵阳', '广州', '2026-11-06 09:00:00', '2026-11-06 10:35:00',  780.00, 150, 150),
('GY1004', '南方航空', '北京', '贵阳', '2026-11-07 14:00:00', '2026-11-07 17:10:00', 1280.00, 180, 180),
('GY1005', '海南航空', '贵阳', '成都', '2026-11-06 19:20:00', '2026-11-06 20:35:00',  620.00, 120, 120),
('GY1006', '中国国航', '上海', '贵阳', '2026-11-08 08:40:00', '2026-11-08 11:55:00', 1150.00, 160, 160);

-- ------------------------------------------------------------
-- 专用账号（避免程序直接用 root）
-- ★ 执行前请把下面两处 'CHANGE_ME_2026' 改成你自己的密码，
--   再把同样的密码填进 config.ini 的 [mysql] password
-- ------------------------------------------------------------
CREATE USER IF NOT EXISTS 'flight'@'localhost' IDENTIFIED BY 'CHANGE_ME_2026';
ALTER USER 'flight'@'localhost' IDENTIFIED BY 'CHANGE_ME_2026';
GRANT SELECT, INSERT, UPDATE, DELETE ON flight_booking.* TO 'flight'@'localhost';
FLUSH PRIVILEGES;

SELECT COUNT(*) AS '航班条数', MIN(depart_time) AS '最早起飞', MAX(depart_time) AS '最晚起飞'
FROM flights;
