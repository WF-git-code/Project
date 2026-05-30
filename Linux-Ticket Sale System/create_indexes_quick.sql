-- ========================================
-- 快速索引优化脚本（必选索引）
-- 执行方式: mysql -u root -p < create_indexes_quick.sql
-- ========================================

USE epoll_project;

-- ticket_table: 查询可用票务
CREATE INDEX idx_ticket_status ON ticket_table(status);

-- yd_table: 查询用户预约记录（高频查询）
CREATE INDEX idx_yd_tel ON yd_table(tel);

-- yd_table: 取消预约时的复合查询
CREATE INDEX idx_yd_tel_tkid ON yd_table(tel, tk_id);

SELECT '关键索引创建完成！' AS Result;
