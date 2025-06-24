#include "detail.hpp"

int main()
{
    DLOG("这是一条DEBUG级别的日志");
    ILOG("这是一条INFO级别的日志");
    ELOG("这是一条ERROR级别的日志");
    return 0;
}
