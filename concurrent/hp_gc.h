#ifndef CONCURRENT_HP_GC_H_
#define CONCURRENT_HP_GC_H_

#include <atomic>
#include <vector>
#include <algorithm>




namespace concurrent
{
struct hazard_pointer
{
    hazard_pointer(bool active) : next_(nullptr), active_(active ? 1 : 0), hazard_(nullptr) {}

    hazard_pointer* next_;
    std::atomic<int> active_;
    std::atomic<void*> hazard_;
};



//static __thread void** rlist = nullptr;
//static __thread size_t rlist_size = 0;
//static const size_t rlist_max_size = 2;

class hazard_pointer_gc
{
public:
    hazard_pointer_gc() : head_(nullptr) {}

    ~hazard_pointer_gc()
    {
        //printf("deleting hazard pointers list\n");

        hazard_pointer* temp = nullptr;
        hazard_pointer* head = head_.load(std::memory_order_relaxed);
        while (head)
        {
            temp = head;
            head = head->next_;

            delete temp;
        }
    }

    hazard_pointer* head()
    {
        //acquire load garantees to read correct values of all members of hazard_pointer structure
        return head_.load(std::memory_order_acquire);
    }

    hazard_pointer* acquire()
    {
        //acquire load garantees to read correct values of next_ pointer
        hazard_pointer* hp = head_.load(std::memory_order_acquire);
        for (; hp; hp = hp->next_)
        {
            int expected = 0;
            if (hp->active_.compare_exchange_strong(expected, 1, std::memory_order_relaxed))
                return hp;
        }

        hp = new hazard_pointer(true);
        hp->next_ = head_.load(std::memory_order_relaxed);

        //publish new member to all
        while(!head_.compare_exchange_strong(hp->next_, hp, std::memory_order_release, std::memory_order_relaxed));
        return hp;
    }


    void release(hazard_pointer* hp)
    {
        //memory order acquire release
        //hazard pointer can be deleted only when all operations on data pointer are done
        hp->hazard_.store(nullptr, std::memory_order_release);
        hp->active_.store(0, std::memory_order_relaxed);
    }

    template<class T, class F>
    size_t scan(T** rlist, size_t size, F deleter)
    {
        std::vector<void*> hp;

        //acquire load garantees to read correct values of next_ pointer
        hazard_pointer* p = head_.load(std::memory_order_acquire);
        for (; p; p = p->next_)
        {
            void* hazard = p->hazard_.load(std::memory_order_relaxed);
            if (hazard)
                hp.push_back(hazard);
        }

        std::sort(std::begin(hp), std::end(hp));

        size_t new_size = size;
        for (size_t i = 0; i < new_size;)
        {
            if (!std::binary_search(std::begin(hp), std::end(hp), rlist[i]))
            {
                //pointer can be safely deleted
                deleter(rlist[i]);
                std::swap(rlist[i], rlist[new_size - 1]);
                --new_size;
            }
            else
            {
                ++i;
            }
        }

        //printf("CLEANUP SUCCEEDED: %d deleted\n", size - new_size);
        return new_size;
    }


private:
    std::atomic<hazard_pointer*> head_;

    //noncopyable
    hazard_pointer_gc(hazard_pointer_gc const&);
    hazard_pointer_gc& operator=(hazard_pointer_gc const&);
};



}//namespace concurrent

#endif //CONCURRENT_HP_GC_H_