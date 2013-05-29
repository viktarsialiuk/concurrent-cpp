#ifndef CONCURRENT_QUEUE_H_
#define CONCURRENT_QUEUE_H_


//Generalized concurrent queue by Herb Sutter
//Please refer to http://www.drdobbs.com/parallel/writing-a-generalized-concurrent-queue/211601363 for more details


#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>          //for std::move
#include <mutex>            //for lock guard

#include "spin_lock.h"      //for spin wait


namespace concurrent
{
template<class T, class A = std::allocator<T> >
class queue
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
    queue()
    {
        first_ = last_ = construct_node(value_type());
    }

    ~queue()
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

        std::lock_guard<spin_lock> guard(producer_lock_);

        //publish to consumer
        last_->next_.store(node, std::memory_order_release);
        last_ = node;
    }

    bool try_pop(value_type& value)
    {
        std::lock_guard<spin_lock> quard(consumer_lock_);

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
    spin_lock consumer_lock_;
    queue_node* first_;

    spin_lock producer_lock_;
    queue_node* last_;

    node_allocator_type alloc_;

    //non copyable
    queue(queue const&);
    queue& operator=(queue const&);
};

}//namespace concurrent



#endif //CONCURRENT_QUEUE_H_