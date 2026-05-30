// 自定义TCP协议头文件
// 协议格式：魔数(4B) + 长度(4B) + 命令(1B) + 数据(NB)
// 解决粘包拆包问题

#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

// 协议魔数，用于标识有效数据包
constexpr uint32_t PROTOCOL_MAGIC = 0xCAFEBABE;

// 协议头大小
constexpr size_t PROTOCOL_HEADER_SIZE = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);

// 协议头结构
struct ProtocolHeader {
    uint32_t magic;     // 魔数 0xCAFEBABE
    uint32_t length;    // 数据长度（不包含协议头）
    uint8_t command;    // 命令类型
};

// 命令类型定义
enum CommandType {
    CMD_LOGIN = 0x01,        // 登录
    CMD_REGISTER = 0x02,     // 注册
    CMD_SHOW_TICKET = 0x03,  // 查看票务
    CMD_ORDER = 0x04,        // 订票
    CMD_CANCEL = 0x05,       // 取消订票
    CMD_SHOW_ORDER = 0x06,   // 查看订单
    CMD_LOGOUT = 0x07,       // 登出
    CMD_EXIT = 0x08          // 退出
};

// 协议处理类
class ProtocolHandler {
private:
    std::vector<char> buffer_;  // 接收缓冲区，用于处理粘包

public:
    ProtocolHandler() {
        buffer_.reserve(4096);  // 预分配4KB缓冲区
    }

    ~ProtocolHandler() = default;

    // 大端序转换：主机字节序 -> 网络字节序
    static uint32_t htonl_custom(uint32_t hostlong) {
        return ((hostlong & 0xFF) << 24) |
               ((hostlong & 0xFF00) << 8) |
               ((hostlong & 0xFF0000) >> 8) |
               ((hostlong & 0xFF000000) >> 24);
    }

    // 大端序转换：网络字节序 -> 主机字节序
    static uint32_t ntohl_custom(uint32_t netlong) {
        return ((netlong & 0xFF) << 24) |
               ((netlong & 0xFF00) << 8) |
               ((netlong & 0xFF0000) >> 8) |
               ((netlong & 0xFF000000) >> 24);
    }

    // 封装协议数据
    static std::string pack(uint8_t command, const std::string& data) {
        std::string result;
        
        // 构建协议头
        ProtocolHeader header;
        header.magic = PROTOCOL_MAGIC;
        header.length = htonl_custom(static_cast<uint32_t>(data.size()));
        header.command = command;
        
        // 添加协议头
        result.append(reinterpret_cast<const char*>(&header), sizeof(header));
        // 添加数据部分
        result.append(data);
        
        return result;
    }

    // 接收数据并解析
    // 返回值：true表示解析到完整数据包，false表示数据不完整
    // output_data: 解析出的业务数据
    // output_cmd: 解析出的命令类型
    bool receive_and_parse(const char* data, size_t len, 
                           std::string& output_data, uint8_t& output_cmd) {
        // 将新数据追加到缓冲区
        buffer_.insert(buffer_.end(), data, data + len);
        
        // 循环尝试解析数据包
        while (buffer_.size() >= PROTOCOL_HEADER_SIZE) {
            // 读取协议头
            ProtocolHeader* header = reinterpret_cast<ProtocolHeader*>(buffer_.data());
            
            // 检查魔数是否正确
            if (header->magic != PROTOCOL_MAGIC) {
                // 魔数不匹配，丢弃一个字节继续找
                buffer_.erase(buffer_.begin());
                continue;
            }
            
            // 获取数据长度（网络字节序转主机字节序）
            uint32_t data_len = ntohl_custom(header->length);
            
            // 计算完整数据包大小
            size_t packet_size = PROTOCOL_HEADER_SIZE + data_len;
            
            // 检查是否有完整的数据
            if (buffer_.size() < packet_size) {
                // 数据不完整，等待更多数据
                break;
            }
            
            // 提取业务数据
            output_cmd = header->command;
            output_data.assign(buffer_.data() + PROTOCOL_HEADER_SIZE, data_len);
            
            // 从缓冲区移除已处理的数据
            buffer_.erase(buffer_.begin(), buffer_.begin() + packet_size);
            
            return true;  // 成功解析一个数据包
        }
        
        return false;  // 没有完整数据包
    }

    // 清空缓冲区
    void clear() {
        buffer_.clear();
    }

    // 获取缓冲区当前大小（用于调试）
    size_t buffer_size() const {
        return buffer_.size();
    }
};

#endif // PROTOCOL_HPP
