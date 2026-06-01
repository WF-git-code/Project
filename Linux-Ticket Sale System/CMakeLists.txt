cmake_minimum_required(VERSION 3.10)
project(epoll_project)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

find_package(Threads REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/server)
include_directories(${PROJECT_SOURCE_DIR}/server/LogFile/include)
include_directories(${PROJECT_SOURCE_DIR}/client)

link_libraries(
    Threads::Threads
    jsoncpp
    mysqlclient
    hiredis
)

# 日志系统源文件
set(LOG_SRC
    server/LogFile/src/AppendFile.cpp
    server/LogFile/src/AsynLogging.cpp
    server/LogFile/src/CountDownLatch.cpp
    server/LogFile/src/LogFile.cpp
    server/LogFile/src/LogMessage.cpp
    server/LogFile/src/Logger.cpp
    server/LogFile/src/Timestamp.cpp
)

add_executable(server
    server/server.cpp
    server/ThreadPool/CachedThreadPool.hpp
    server/ThreadPool/TaskQueue.hpp
    ${LOG_SRC}
)

add_executable(client
    client/client.cpp
)

add_executable(log_demo
    demo/LogCleanerDemo.cpp
)


