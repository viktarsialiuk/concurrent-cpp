#include <iostream>
#include "api.h"

//static const size_t kMaxRetiredListSize = 100;
//__declspec(thread) int array[kMaxRetiredListSize] = {0};

using namespace std;



int main(int argc, char* argv[])
{
    cout << "start" << endl;
    test_api::run();

    cout << "done" << endl;
    std::cin.get();
    return 0;
}