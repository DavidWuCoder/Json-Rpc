/*
 * 实现项目中一些琐碎功能的代码
 * 日志宏实现
 * json的序列化和反序列化
 * uuid生成
 */
#pragma once
#include <stdio.h>
#include <time.h>

// 定义ANSI颜色转义序列
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"

// 日志级别定义
#define LDEBUG 0
#define LINFO 1
#define LERROR 2

#define LDEFAULT LDEBUG

// 带颜色的日志宏定义 - 修复了可变参数处理
#define LOG(LEVEL, COLOR, format, ...)                                       \
    {                                                                        \
        if (LEVEL >= LDEFAULT)                                               \
        {                                                                    \
            time_t t = time(NULL);                                           \
            struct tm *lt = localtime(&t);                                   \
            char time_tmp[32] = {0};                                         \
            strftime(time_tmp, 31, "%m-%d %T", lt);                          \
            fprintf(stdout, "%s[%s]:[%s:%d]" format "%s\n", COLOR, time_tmp, \
                    __FILE__, __LINE__, ##__VA_ARGS__, COLOR_RESET);         \
        }                                                                    \
    }

// 不同级别的日志宏
#define DLOG(format, ...) LOG(LDEBUG, COLOR_CYAN, format, ##__VA_ARGS__)
#define ILOG(format, ...) LOG(LINFO, COLOR_GREEN, format, ##__VA_ARGS__)
#define ELOG(format, ...) LOG(LERROR, COLOR_RED, format, ##__VA_ARGS__)
