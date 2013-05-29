#include <iostream>

void queue_test();
void stack_test();
void spin_lock_test();

int main(int argc, char* argv[])
{
    queue_test();
    //stack_test();
    //spin_lock_test();

    std::cin.get();
    return 0;
}