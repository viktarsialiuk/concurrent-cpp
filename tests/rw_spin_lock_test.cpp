#include <iostream>

#include "api.h"
#include "../concurrent/rw_spin_lock.h"

using namespace concurrent;
using namespace std;

void rw_spin_lock_test()
{
    cout << "rw_spin_lock_test" << endl;
}

REGISTER_TEST(rw_spin_lock_test);