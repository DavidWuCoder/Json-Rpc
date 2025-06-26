#include "detail.hpp"

int main() {
    for (int i = 0; i < 10; i++) {
        std::cout << wylrpc::UUID::uuid() << std::endl;
    }
    return 0;
}
