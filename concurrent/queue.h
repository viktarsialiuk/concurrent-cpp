#ifndef CONCURRENT_QUEUE_H_
#define CONCURRENT_QUEUE_H_




#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>          //for std::move
#include <assert.h>

#include "spin_lock.h"      //for spin wait


namespace concurrent
{
//mpmc queue by Herb Sutter
//Please refer to http://www.drdobbs.com/parallel/writing-a-generalized-concurrent-queue/211601363 for more details

template<class T, class A = std::allocator<T> >
class mpmc_queue
{
    struct queue_node;

public:
    typedef T                                                   value_type;
    typedef A                                                   allocator_type;
    typedef typename A::template rebind<queue_node>::other      node_allocator_type;

private:
    struct queue_node
    {
        queue_node(value_type const& data) : data_(data), next_(nullptr) {}

        value_type data_;
        //typename std::aligned_storage<
        //    sizeof(value_type),
        //    std::alignment_of<value_type>::value
        //>::type data_;

        std::atomic<queue_node*> next_;
    };

public:
    mpmc_queue()
    {
        first_ = last_ = construct_node(value_type());
    }

    ~mpmc_queue()
    {
        queue_node* node = first_;
        while (node)
        {
            queue_node* temp = node;
            node = node->next_.load(std::memory_order_relaxed);

            destroy_node(temp);
        }
    }

    void push(value_type const& value)
    {
        queue_node* node = construct_node(value);

        spin_lock::lock_guard guard(producer_lock_);

        //publish to consumer
        last_->next_.store(node, std::memory_order_release);
        last_ = node;
    }

    bool try_pop(value_type& value)
    {
        spin_lock::lock_guard guard(consumer_lock_);

        queue_node* first = first_;
        //acquire read synchronizes with release store in producer thread
        //garanteed to read correct value of queue_node::data_
        queue_node* next = first_->next_.load(std::memory_order_acquire);
        if (next)
        {
            value = std::move(next->data_);
            first_ = next;

            destroy_node(first);
            return true;
        }

        return false;
    }


private:
    queue_node* construct_node(value_type const& value)
    {
        queue_node* node = alloc_.allocate(1);

        try
        {
            new (node) queue_node(value);
        }
        catch(std::exception&)
        {
            alloc_.deallocate(node, 1);
            throw;
        }

        return node;
    }

    void destroy_node(queue_node* node)
    {
        if (node)
        {
            node->~queue_node();
            alloc_.deallocate(node, 1);
        }
    }

private:
    static const size_t kCacheLineSize = 64;
    typedef char cache_line_pad[kCacheLineSize];

    spin_lock consumer_lock_;
    cache_line_pad pad0;

    queue_node* first_;
    cache_line_pad pad1;

    spin_lock producer_lock_;
    cache_line_pad pad2;

    queue_node* last_;
    cache_line_pad pad3;

    node_allocator_type alloc_;
    cache_line_pad pad4;

    //non copyable
    mpmc_queue(mpmc_queue const&);
    mpmc_queue& operator=(mpmc_queue const&);
};

//spsc queue based on ring buffer
template<class T, size_t C, class A = std::allocator<T> >
class spsc_bounded_queue
{
    struct queue_node;

    enum
    {
        kCapacity = C,
        kBufferMask = C - 1
    };
public:
    typedef T   value_type;
    typedef A   allocator_type;

private:
    //struct queue_node
    //{
    //    queue_node() : active_(0) {}

    //    value_type data_;
    //    //typename std::aligned_storage<
    //    //    sizeof(value_type),
    //    //    std::alignment_of<value_type>::value
    //    //>::type data_;

    //    std::atomic<int> active_;
    //};

public:
    spsc_bounded_queue();
    ~spsc_bounded_queue();

    bool try_push(value_type const& value);
    bool try_pop(value_type& value);

private:
    void construct_node(value_type* addr, value_type const& value)
    {
        new (addr) value_type(value);
    }
    void destroy_node(value_type* addr) //noexcept
    {
        addr->~value_type();
    }
    size_t next_index(size_t index) const //noexcept
    {
        return (index + 1) & kBufferMask;
    }


private:
    static const size_t kCacheLineSize = 64;
    typedef char cache_line_pad[kCacheLineSize];

    std::atomic<int> head_;
    cache_line_pad pad0;

    std::atomic<int> tail_;
    cache_line_pad pad1;

    value_type* buffer_;
    cache_line_pad pad2;

    allocator_type alloc_;
    cache_line_pad pad3;

    //noncopyable
    spsc_bounded_queue(spsc_bounded_queue const&);
    spsc_bounded_queue& operator=(spsc_bounded_queue const&);
};

//spsc_bounded_queue inline implementation
template<class T, size_t C, class A>
spsc_bounded_queue<T, C, A>::spsc_bounded_queue()
    :
    buffer_(nullptr), //static_cast<value_type*>(::operator new(sizeof(value_type) * Capacity))
    head_(0), tail_(0)
{
    //static_assert(Capacity > 0)
    //make sure capacity is power of 2
    buffer_ = alloc_.allocate(kCapacity);
}


template<class T, size_t C, class A>
spsc_bounded_queue<T, C, A>::~spsc_bounded_queue()
{
    static_assert(kCapacity > 0 && (kCapacity & (kCapacity - 1)) == 0, "Capacity must be positive power of two");

    size_t head = head_.load(std::memory_order_relaxed);
    size_t tail = tail_.load(std::memory_order_relaxed);

    for(; tail != head; tail = next_index(tail))
    {
        destroy_node(&buffer_[tail]);
    }

    //::operator delete(buffer_);
    alloc_.deallocate(buffer_, kCapacity);
}

template<class T, size_t C, class A>
bool spsc_bounded_queue<T, C, A>::try_push(value_type const& value)
{
    size_t head = head_.load(std::memory_order_relaxed);
    //tail_.load(std::memory_order_acquire) synchronizes with release store in pop
    //but doesn't garantee to read the most preceding value
    //that's why push can occasionally fail to enqueue new value even if queue is not full
    //that's why you need to enqueue value in a loop
    size_t tail = tail_.load(std::memory_order_acquire);
    size_t next = next_index(head);

    if (next == tail) //queue is full
        return false;

    construct_node(&buffer_[head], value);

    head_.store(next, std::memory_order_release);
    return true;
}

template<class T, size_t C, class A>
bool spsc_bounded_queue<T, C, A>::try_pop(value_type& value)
{
    size_t tail = tail_.load(std::memory_order_relaxed);
    //head_.load(std::memory_order_acquire) synchronizes with release store in push
    //but doesn't garantee to read the most preceding value
    //that's why pop can occasionally fail to dequeue value even if queue is not empty
    //that's why you need to dequeue value in a loop
    size_t head = head_.load(std::memory_order_acquire);
    size_t next = next_index(tail);

    if (tail == head) //queue is empty
        return false;

    value = buffer_[tail];
    destroy_node(&buffer_[tail]);

    tail_.store(next, std::memory_order_release);
    return true;
}


}//namespace concurrent



#endif //CONCURRENT_QUEUE_H_