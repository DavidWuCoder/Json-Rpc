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
    // 1.封装任务
    auto task = std::make_shared<std::packaged_task<int(int, int)>>(Add);

    // 2.获取关联的future对象
    std::future<int> res = task->get_future();

    std::thread thr([task]() { (*task)(11, 22); });

    std::cout << res.get() << std::endl;
    thr.join();
    return 0;
}
