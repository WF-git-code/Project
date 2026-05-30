-- ========================================
-- 预约票务系统索引优化脚本
-- 创建时间: 2024-01-20
-- 说明: 为高并发场景优化数据库查询性能
-- ========================================

USE epoll_project;

-- ========================================
-- 1. user_info 表索引优化
-- ========================================

-- 为电话登录查询添加索引（如果还没有）
CREATE INDEX IF NOT EXISTS idx_user_tel ON user_info(Tel);

-- 为时间范围查询添加索引（如果按注册时间查询）
CREATE INDEX IF NOT EXISTS idx_user_ztime ON user_info(Ztime);

-- ========================================
-- 2. ticket_table 表索引优化（关键！）
-- ========================================

-- 索引1: status字段 - 查询可用票务时使用
-- 查询: SELECT ... FROM ticket_table WHERE status=1
CREATE INDEX IF NOT EXISTS idx_ticket_status ON ticket_table(status);

-- 索引2: datetime字段 - 按日期查询票务时使用
-- 查询: SELECT ... FROM ticket_table WHERE datetime='2024-01-20'
CREATE INDEX IF NOT EXISTS idx_ticket_datetime ON ticket_table(datetime);

-- 索引3: status+datetime复合索引 - 查询某天可用的票务
-- 查询: SELECT ... FROM ticket_table WHERE status=1 AND datetime='2024-01-20'
CREATE INDEX IF NOT EXISTS idx_ticket_status_datetime ON ticket_table(status, datetime);

-- ========================================
-- 3. yd_table 表索引优化（关键！）
-- ========================================

-- 索引1: tel字段 - 查询用户的预约记录时使用
-- 查询: SELECT ... FROM yd_table WHERE tel='13800138000'
CREATE INDEX IF NOT EXISTS idx_yd_tel ON yd_table(tel);

-- 索引2: tk_id字段 - 查询某票务的所有预约时使用
-- 查询: SELECT ... FROM yd_table WHERE tk_id=1
CREATE INDEX IF NOT EXISTS idx_yd_tkid ON yd_table(tk_id);

-- 索引3: tel+tk_id复合索引 - 取消预约时使用（关键！）
-- 查询: UPDATE yd_table SET status=0 WHERE tel='13800138000' AND tk_id=1 AND status=1
CREATE INDEX IF NOT EXISTS idx_yd_tel_tkid ON yd_table(tel, tk_id);

-- 索引4: status字段 - 查询特定状态的预约记录时使用
CREATE INDEX IF NOT EXISTS idx_yd_status ON yd_table(status);

-- ========================================
-- 4. 查看创建的所有索引
-- ========================================
SHOW INDEX FROM user_info;
SHOW INDEX FROM ticket_table;
SHOW INDEX FROM yd_table;

-- ========================================
-- 5. 分析查询性能（可选）
-- ========================================
-- 使用 EXPLAIN 分析查询是否使用索引
-- 示例:
-- EXPLAIN SELECT tk_id, tk_name, tk_max, tk_count, datetime 
-- FROM ticket_table WHERE status=1;

-- EXPLAIN SELECT yd_id, tel, tk_id, ctime, status 
-- FROM yd_table WHERE tel='13800138000';

-- ========================================
-- 6. 索引维护建议
-- ========================================
-- 
-- 查看索引使用情况:
-- SHOW STATUS LIKE 'Handler_read%';
--
-- 分析表:
-- ANALYZE TABLE user_info;
-- ANALYZE TABLE ticket_table;
-- ANALYZE TABLE yd_table;
--
-- 优化表:
-- OPTIMIZE TABLE ticket_table;
-- OPTIMIZE TABLE yd_table;

SELECT '索引创建完成！' AS Result;
