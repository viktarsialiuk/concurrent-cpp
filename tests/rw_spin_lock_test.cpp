#include <iostream>

#include "api.h"
#include "../concurrent/rw_spin_lock.h"

using namespace concurrent;
using namespace std;

static const int kMaxSteps = 1000;

int data;
rw_spin_lock lock;

static void consumer()
{
    int value = 0;
    while (value < kMaxSteps)
    {
        lock.lock_shared();
        value = data;
        lock.unlock_shared();
    }

    cout << "consumer stopped\n";
}

static void producer()
{
    for (int i = 0; i < kMaxSteps; ++i)
    {
        lock.lock();
        data = i + 1;
        lock.unlock();
        this_thread::sleep_for(std::chrono::microseconds(10));
    }
    cout << "producer stopped\n";
}


void rw_spin_lock_test()
{
    cout << "rw_spin_lock_test" << endl;

    std::thread thc1(consumer);
    std::thread thc2(consumer);
    std::thread thc3(consumer);
    std::thread thp(producer);

    thc1.join();
    thc2.join();
    thc3.join();
    thp.join();
}

REGISTER_TEST(rw_spin_lock_test);