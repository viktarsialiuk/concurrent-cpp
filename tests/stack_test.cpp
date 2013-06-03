#include <iostream>
#include <thread>

#include "api.h"
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
        //std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    //cout << "producer stopped\n";
}

static void consumer()
{
    int items_count = 0;
    for (int i = 0; i < STEPS; ++i)
    {
        int value = 0;
        data.pop(value);
        //cout << value << " ";
    }
    //cout << "consumer stopped\n";
}

void stack_test()
{
    cout << "stack_test" << endl;

    std::thread thc(consumer);
    std::thread thp(producer);

    thc.join();
    thp.join();
}

REGISTER_TEST(stack_test);