#include <iostream>
#include <unistd.h>
#include <bits/this_thread_sleep.h>

int main() {
    std::cout << "current process PID=" << getpid() << std::endl;
    int i = 0, summa = 0;
    while (i < 100) {
        summa += i;
        i++;
        std::cout << summa << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
