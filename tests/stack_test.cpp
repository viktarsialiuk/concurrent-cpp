#include <iostream>
#include <thread>

#include "../concurrent/stack.h"

using namespace concurrent;
using namespace std;

concurrent::stack<int> data;

static const int STEPS = 10000;
static void producer()
{
    for (int i = 0; i < STEPS; ++i)
    {
        data.push(i);
        //sleep for 10 microseconds to allow consumer to take data
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    cout << "\nproducer stopped\n";
}

static void consumer()
{
    int items_count = 0;
    for (int i = 0; i < STEPS; ++i)
    {
        int value = 0;
        data.pop(value);
        cout << value << " ";
    }
    cout << "\nconsumer stopped\n";
}

void stack_test()
{
    std::thread thc(consumer);
    std::thread thp(producer);

    thc.join();
    thp.join();
}