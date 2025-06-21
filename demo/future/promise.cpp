#include <chrono>
#include <future>
#include <iostream>
#include <thread>

int Add(int x, int y)
{
    std::cout << "into add! " << std::endl;
    return x + y;
}

int main()
{
    // 1. 使用 shared_ptr 管理 promise 对象
    auto pro = std::make_shared<std::promise<int>>();

    // 2. 获取关联的 future 对象
    std::future<int> res = pro->get_future();

    // 3. 创建线程并执行异步任务
    std::thread thr(
        [pro]()
        {
            int sum = Add(11, 22);
            pro->set_value(sum);
        });

    // 4. 获取异步计算的结果
    std::cout << res.get() << std::endl;

    // 5. 等待线程完成
    thr.join();
    return 0;
}
