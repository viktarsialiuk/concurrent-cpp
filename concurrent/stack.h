#ifndef CONCURRENT_STACK_H_
#define CONCURRENT_STACK_H_

#include <atomic>
#include <memory>
#include "spin_lock.h"

namespace concurrent
{
    template<class T, class A = std::allocator<T> >
class stack
{
    struct stack_node;

public:
    typedef T                                                   value_type;
    typedef stack_node*                                         stack_node_ptr;
    typedef A                                                   allocator_type;
    typedef typename A::template rebind<stack_node>::other      node_allocator_type;

private:
    struct stack_node
    {
        value_type  data_;
        stack_node* next_;
    };

public:
    stack() : head_(0) {}

    ~stack()
    {
        stack_node* node = head_.load(std::memory_order_relaxed);
        while (node)
        {
            stack_node* temp = node;
            node = node->next_;

            destroy_node(node);
        }
    }


    //EXCEPTION SAFE
    bool try_pop(value_type& value)
    {
        stack_node* prev_head;

        //acquire load, granteed to read correct value of next_ pointer and data_
        //should be changed to memory_order_consume
        while (prev_head = head_.load(std::memory_order_acquire))
        {
            if (head_.compare_exchange_strong(prev_head, prev_head->next_, std::memory_order_relaxed))
                break;
        }


        if (!prev_head)
            return false;

        try
        {
            value = prev_head->data_;
        }
        catch(std::exception&)
        {
            destroy_node(prev_head);
            throw;
        }

        destroy_node(prev_head);
        return true;
    }



    void push(value_type const& value)
    {
        stack_node* new_head = construct_node(value);
        stack_node* prev_head;
        do
        {
            prev_head = head_.load(std::memory_order_relaxed);
            new_head->next_ = prev_head;
        }
        while (!head_.compare_exchange_strong(prev_head, new_head, std::memory_order_release, std::memory_order_relaxed));
    }

    //void push(value_type&& value){}


private:
    stack_node* construct_node(value_type const& value)
    {
        stack_node* node = alloc_.allocate(1);
        try
        {
            new (&node->next_) stack_node_ptr(0);//nothrow
            new (&node->data_) value_type(value);

        }
        catch (std::exception&)
        {
            alloc_.deallocate(node, 1);
            throw;
        }

        return node;
    }

    void destroy_node(stack_node* node)
    {
        if (node)
        {
            node->data_.~value_type();
            node->next_.~stack_node_ptr();
            alloc_.deallocate(node, 1);
        }
    }

private:
    std::atomic<stack_node*> head_;

    //allocator_type alloc_;
    node_allocator_type alloc_;

    //noncopyable
    stack(stack const&);
    stack& operator=(stack const&);
};

}//namespace concurrent

#endif //CONCURRENT_STACK_H_