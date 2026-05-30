-- 创建数据库
CREATE DATABASE IF NOT EXISTS epoll_project DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;

USE epoll_project;

-- 用户信息表
CREATE TABLE IF NOT EXISTS user_info(
    UserId INT PRIMARY KEY NOT NULL UNIQUE AUTO_INCREMENT,
    Tel CHAR(11) UNIQUE,
    Name VARCHAR(20) NOT NULL,
    Passwd VARCHAR(8) NOT NULL,
    Status TINYINT,
    Ztime DATE
);

-- 票务信息表
CREATE TABLE IF NOT EXISTS ticket_table(
    tk_id INT PRIMARY KEY NOT NULL UNIQUE AUTO_INCREMENT,
    tk_name VARCHAR(20) NOT NULL,
    tk_max INT,
    tk_count INT,
    datetime DATE,
    status TINYINT
);

-- 预约信息表
CREATE TABLE IF NOT EXISTS yd_table(
    yd_id INT PRIMARY KEY NOT NULL UNIQUE AUTO_INCREMENT,
    tel CHAR(11),
    tk_id INT,
    ctime DATETIME,
    status TINYINT
);

-- 插入测试数据
INSERT INTO ticket_table (tk_name, tk_max, tk_count, datetime, status) VALUES
('电影票-A厅', 100, 10, '2024-01-15', 1),
('演唱会门票', 500, 200, '2024-02-01', 1),
('话剧演出', 200, 50, '2024-01-20', 1);
