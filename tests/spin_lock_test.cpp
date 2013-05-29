#include <iostream>
#include <mutex>
#include <deque>
#include "../concurrent/spin_lock.h"

using namespace concurrent;
using namespace std;

spin_lock mx;
deque<int> data;

static const int STEPS = 100000;
static void producer()
{
    for (int i = 0; i < STEPS; ++i)
    {
        std::lock_guard<spin_lock> guard(mx);
        data.push_back(i);
    }

    {
        std::lock_guard<spin_lock> guard(mx);
        data.push_back(-1);
    }
}

static void consumer()
{
    int res = 0;
    while (res >= 0)
    {
        {
            std::lock_guard<spin_lock> guard(mx);
            if (!data.empty())
            {
                res = data.front();
                data.pop_front();
                cout << res << " ";
            }
        }
    }
}

void spin_lock_test()
{
    std::thread thc(consumer);
    std::thread thp(producer);

    thc.join();
    thp.join();
}