#ifndef CONCURRENT_RW_SPIN_LOCK_H_
#define CONCURRENT_RW_SPIN_LOCK_H_
#include <stdio.h>
#include <atomic>
#include <thread>

#include "spin_wait.h"

namespace concurrent
{


//reader writer spin lock implementation
//ported from TBB implementation
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
        state_.fetch_and(kReaders, std::memory_order_release);
    }

    void lock_shared()
    {
        spin_wait([this]() { return try_lock_shared(); });
    }

    void unlock_shared()
    {
        state_.fetch_add(-kReader, std::memory_order_release);
    }


    bool try_lock()
    {
        int state = state_.load(std::memory_order_relaxed);
        if (!(state & kBusy))
        {
            //OK try to acquire writer lock using CAS
            return state_.compare_exchange_strong(state, kWriter, std::memory_order_acquire, std::memory_order_relaxed);
        }
        else if (!(state & KPendingWriter))
        {
            //writer has higher priority than readers
            state_.fetch_or(KPendingWriter, std::memory_order_relaxed);
        }
        return false;
    }

    bool try_lock_shared()
    {
        int state = state_.load(std::memory_order_relaxed);
        if (!(state & (KPendingWriter | kWriter)))
        {
            //ok no writers and no pending writers
            //try to acquire reader lock using fetch add instead of CAS
            //return state_.compare_exchange_strong(state, state + kReader, std::memory_order_acquire, std::memory_order_relaxed);

            state = state_.fetch_add(kReader, std::memory_order_acquire);
            if (!(state & kWriter))
                return true;

            //unlock reader
            state_.fetch_add(-kReader, std::memory_order_release);
        }
        return false;
    }

private:
    std::atomic<int> state_;

    //noncopyable
    rw_spin_lock(rw_spin_lock const&);
    rw_spin_lock& operator=(rw_spin_lock const&);
};

}//namespace concurrent


#endif //CONCURRENT_RW_SPIN_LOCK_H_