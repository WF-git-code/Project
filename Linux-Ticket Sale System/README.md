# 预约系统 - 文档中心

欢迎来到预约系统文档中心！

## 文档列表



| 文档名称 | 文件名 | 说明 |
|---------|--------|------|
| 项目总览 | [README.md](README.md) | 项目整体介绍、日志清理功能说明 |
| 系统配置指南 | [系统配置指南.md](系统配置指南.md) | 完整的配置项说明、场景配置方案 |
| 线程池配置指南 | [线程池配置指南.md](线程池配置指南.md) | 线程池参数详解、性能优化建议 |
| 日志清理使用说明 | [日志清理使用说明.md](日志清理使用说明.md) | 日志清理功能详细说明 |

##  快速开始

### 新手推荐阅读顺序

1. [项目总览](README.md) - 了解项目整体
2. [系统配置指南](系统配置指南.md) - 掌握配置方法docs目录下
3. [线程池配置指南](线程池配置指南.md) - 优化性能docs目录下
4. [日志清理使用说明](日志清理使用说明.md) - 管理日志docs目录下

## 配置示例速查

### 开发环境

```ini
ip 127.0.0.1
port 6000
lismax 512
corethreadnum 2
maxthreadnum 8
taskmax 512
taskwaittime 10
threadwaittime 10
enablelogclean 1
logkeepdays 7
logdir ./
```

##

##  相关链接

- [配置文件示例](../my.conf)
- [日志清理演示程序](../demo/LogCleanerDemo.cpp)

---

运行

c/c++ 环境    linux   Ubuntu20.4包

注意redis配置和MySQL配置

数据库		版本		说明
Redis		5.0.7		hiredis 5.2.1
MySQL		8.0.42		Ubuntu 20.04 包

```bash
cd build 
make clean
make
cd ../bin
./server
./client
```

