/*
 * 实现项目中一些琐碎功能的代码
 * 日志宏实现
 * json的序列化和反序列化
 * uuid生成
 */
#pragma once
#include <jsoncpp/json/json.h>

#include <atomic>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>

namespace wylrpc {
// 定义ANSI颜色转义序列
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"
#define COLOR_NONE ""

// 日志级别定义
#define LDEBUG 0
#define LINFO 1
#define LERROR 2

#define LDEFAULT LDEBUG

// 带颜色的日志宏定义 - 修复了可变参数处理
#define LOG(LEVEL, COLOR, format, ...)                                       \
    {                                                                        \
        if (LEVEL >= LDEFAULT) {                                             \
            time_t t = time(NULL);                                           \
            struct tm *lt = localtime(&t);                                   \
            char time_tmp[32] = {0};                                         \
            strftime(time_tmp, 31, "%m-%d %T", lt);                          \
            fprintf(stdout, "%s[%s]:[%s:%d]" format "%s\n", COLOR, time_tmp, \
                    __FILE__, __LINE__, ##__VA_ARGS__, COLOR_RESET);         \
        }                                                                    \
    }

// 不同级别的日志宏
#define DLOG(format, ...) LOG(LDEBUG, COLOR_NONE, format, ##__VA_ARGS__)
#define ILOG(format, ...) LOG(LINFO, COLOR_GREEN, format, ##__VA_ARGS__)
#define ELOG(format, ...) LOG(LERROR, COLOR_RED, format, ##__VA_ARGS__)

class JsonUtil {
    // 序列化
public:
    static bool serialize(const Json::Value &val, std::string &body) {
        // 先得有工厂类
        Json::StreamWriterBuilder swb;
        swb.settings_["emitUTF8"] = true;  // 加上这句防止输出乱码
        // 通过工厂类创建StreamBuiler
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());

        std::stringstream ss;
        int ret = sw->write(val, &ss);
        if (ret != 0) {
            return false;
        }
        body = ss.str();
        return true;
    }

    // 反序列化
    static bool unserialize(const std::string &body, Json::Value &val) {
        // 先定义工厂类
        Json::CharReaderBuilder crb;
        crb.settings_["emitUTF8"] = true;  // 禁止值转义为Unicode
        std::unique_ptr<Json::CharReader> cr(crb.newCharReader());

        std::string errs;
        bool ret =
            cr->parse(body.c_str(), body.c_str() + body.size(), &val, &errs);
        if (ret == false) {
            ELOG("json unserialize failed : %s", errs.c_str());
            return false;
        }
        return true;
    }
};
class UUID {
public:
    static std::string uuid() {
        std::stringstream ss;
        // 1.机器随机数
        std::random_device rd;
        // 2.伪随机数
        std::mt19937 generator(rd());
        // 3.限定随机数范围
        std::uniform_int_distribution<int> distribution(0, 255);
        // 4.生成8各随机数，按照特定格式组织成为16进制数字字符的字符串
        for (int i = 0; i < 8; i++) {
            if (i == 4 || i == 6) {
                ss << "-";
            }
            ss << std::setw(2) << std::setfill('0') << std::hex
               << distribution(generator);
        }
        ss << "-";
        //  5.定义一个8字节序号，按照特定格式组织成为16进制数字字符的字符串
        static std::atomic<size_t> seq(1);
        size_t cur = seq.fetch_add(1);
        for (int i = 7; i >= 0; i--) {
            if (i == 5) ss << "-";
            ss << std::setw(2) << std::setfill('0') << std::hex
               << ((cur >> (i * 8)) & 0xFF);
        }
        return ss.str();
    }
};
}  // namespace wylrpc
