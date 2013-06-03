#ifndef CONCURRENT_RW_SPIN_LOCK_H_
#define CONCURRENT_RW_SPIN_LOCK_H_
#include <atomic>
#include <thread>

#include "spin_wait.h"

namespace concurrent
{


//naive spin lock implementation
class rw_spin_lock
{
    enum
    {
        kWriter = 0x1,
        KPendingWriter = 0x2,
        kReader = 0x4,
        kReaders = ~(kWriter | KPendingWriter),
        kBusy = kWriter | kReaders
    };
public:
    rw_spin_lock() : state_(0) {}

    void lock()
    {
        spin_wait([this]() { return try_lock(); });
    }

    void unlock()
    {
        state_.store(0, std::memory_order_release);
    }

    void lock_reader()
    {
        spin_wait([this]() { return try_lock_reader(); });
    }

    void unlock_reader()
    {
        state_.fetch_add(-kReader, std::memory_order_release);
    }


    bool try_lock()
    {
        int state = 0;
        if (!state_.compare_exchange_strong(state, kWriter, std::memory_order_acquire, std::memory_order_relaxed))
        {
            return false;
        }
        return true;
    }

    bool try_lock_reader()
    {
        int state = state_.fetch_add(kReader, std::memory_order_acquire);
        if ((state & (kWriter | KPendingWriter)))
        {
            state_.fetch_add(-kReader, std::memory_order_release);
            return false;
        }
        return true;
    }


private:
    std::atomic<int> state_;

    //noncopyable
    rw_spin_lock(rw_spin_lock const&);
    rw_spin_lock& operator=(rw_spin_lock const&);
};

}//namespace concurrent


#endif //CONCURRENT_RW_SPIN_LOCK_H_