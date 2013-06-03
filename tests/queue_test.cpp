#include <iostream>
#include <thread>

#include "api.h"
#include "../concurrent/queue.h"

using namespace concurrent;
using namespace std;

concurrent::queue<int> data;


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
    while (items_count < STEPS)
    {
        int value = 0;
        if (data.try_pop(value))
        {
            ++items_count;
            //cout << value << " ";
        }
        else
        {
            //std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }
    //cout << "consumer stopped\n";
}


void queue_test()
{
    cout << "queue_test" << endl;

    std::thread thc(consumer);
    std::thread thp(producer);

    thc.join();
    thp.join();
}


REGISTER_TEST(queue_test);
