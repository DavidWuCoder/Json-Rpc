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
    std::future<int> res = std::async(std::launch::async, Add, 11, 22);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "-----------------------" << std::endl;
    std::cout << res.get() << std::endl;
    return 0;
}
