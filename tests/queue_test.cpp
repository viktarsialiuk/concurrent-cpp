#include <iostream>
#include <thread>

#include <intrin.h>


#include "api.h"
#include "../concurrent/queue.h"

using namespace concurrent;
using namespace std;


concurrent::spsc_bounded_queue<int, 1024*1024> data;
concurrent::mpmc_queue<int> data1;

std::atomic<int> flag(0);

static const int kSteps = 10000000;

static void producer()
{
    while (flag.load(std::memory_order_relaxed) == 0)
        std::this_thread::yield();

    int item = 0, failed = 0;
    for (int i = 0; i < kSteps; ++i)
    {
        while(!data.try_push(i))
        {
            //++failed;
            std::this_thread::yield();
        }
    }
    //cout << "failed inserts " << failed;
}

static void consumer()
{
    while (flag.load(std::memory_order_relaxed) == 0)
        std::this_thread::yield();


    int items_count = 0;
    //while (items_count < kSteps)
    int value = 0;
    for (int i = 0; i < kSteps;)
    {
        if (data.try_pop(value))
        {
            ++i;
            //++items_count;
            //cout << value << " ";
        }
        else
        {
            std::this_thread::yield();
        }
    }
    //cout << "consumer stopped\n";
}


void queue_test()
{
    cout << "queue_test:";

    std::thread thc(consumer);
    std::thread thp(producer);

    unsigned long long start = __rdtsc();
    flag.store(1, std::memory_order_relaxed);

    thc.join();
    thp.join();

    unsigned long long time = __rdtsc() - start;
    std::cout << " cycles/op=" << time / (kSteps * 2) << std::endl;
}


REGISTER_TEST(queue_test);
