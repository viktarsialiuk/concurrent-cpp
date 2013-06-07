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

    //hazard_pointer* head()
    //{
    //    //acquire load garantees to read correct values of all members of hazard_pointer structure
    //    return head_.load(std::memory_order_acquire);
    //}

    hazard_pointer* acquire()
    {
        //acquire load garantees to read correct values of next_ pointer
        //but we do not need sequential consistency here
        hazard_pointer* hp = head_.load(std::memory_order_seq_cst);
        for (; hp; hp = hp->next_)
        {
            int expected = 0;
            if (hp->active_.compare_exchange_strong(expected, 1, std::memory_order_relaxed))
                return hp;
        }

        hp = new hazard_pointer(true);
        hp->next_ = head_.load(std::memory_order_relaxed);

        //publish new member to all
        while(!head_.compare_exchange_strong(hp->next_, hp, std::memory_order_seq_cst, std::memory_order_relaxed));
        return hp;
    }


    void release(hazard_pointer* hp)
    {
        hp->hazard_.store(nullptr, std::memory_order_seq_cst);
        hp->active_.store(0, std::memory_order_relaxed);
    }

    template<class T, class F>
    size_t scan(T** rlist, size_t rlist_size, F deleter)
    {
        std::vector<void*> hp;

        //acquire load garantees to read correct values of next_ pointer
        //but does not garantee to read value of the last preceding modification of head_
        //that's why seq_cst memory order must be used
        hazard_pointer* p = head_.load(std::memory_order_seq_cst);
        for (; p; p = p->next_)
        {
            //seq_cst load garantee to read the last memory_order_seq_cst modification or later a modification
            void* hazard = p->hazard_.load(std::memory_order_seq_cst);
            if (hazard)
                hp.push_back(hazard);
        }

        std::sort(std::begin(hp), std::end(hp));

        size_t size = rlist_size;
        for (size_t i = 0; i < size;)
        {
            if (!std::binary_search(std::begin(hp), std::end(hp), rlist[i]))
            {
                //pointer can be safely deleted
                deleter(rlist[i]);
                std::swap(rlist[i], rlist[size - 1]);
                --size;
            }
            else
            {
                ++i;
            }
        }

        //printf("CLEANUP SUCCEEDED: %d deleted\n", size - new_size);
        return size;
    }


private:
    std::atomic<hazard_pointer*> head_;

    //noncopyable
    hazard_pointer_gc(hazard_pointer_gc const&);
    hazard_pointer_gc& operator=(hazard_pointer_gc const&);
};



}//namespace concurrent

#endif //CONCURRENT_HP_GC_H_