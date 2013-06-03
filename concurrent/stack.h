#ifndef CONCURRENT_STACK_H_
#define CONCURRENT_STACK_H_

#include <atomic>
#include <memory>

#include "spin_wait.h"
#include "hp_gc.h"

namespace concurrent
{


template<class T, class A = std::allocator<T> >
class stack
{
    struct stack_node;

public:
    typedef T                                                   value_type;
    typedef A                                                   allocator_type;
    typedef typename A::template rebind<stack_node>::other      node_allocator_type;

private:
    struct stack_node
    {
        stack_node(value_type const& data) : data_(data), next_(nullptr) {}

        value_type  data_;
        stack_node* next_;
    };

public:
    stack() : head_(nullptr) {}

    ~stack()
    {
        stack_node* node = head_.load(std::memory_order_relaxed);
        while (node)
        {
            stack_node* temp = node;
            node = node->next_;
            destroy_node(temp);
        }
    }


    void pop(value_type& value)
    {
        stack_node* head;
        spin_wait([this, &head](){ return try_pop_head(&head); });

        //take ownership of head pointer
        auto deleter = [this](stack_node* node) { destroy_node(node); };
        std::unique_ptr<stack_node, decltype(deleter)> head_owner(head, deleter);

        value = std::move(head->data_);
    }

    //TODO:think how to implement exception safe stack::pop without try catch
    bool try_pop(value_type& value)
    {
        stack_node* head;
        hazard_pointer* hp = gc_.acquire();
        do
        {
            do
            {
                head = head_.load(std::memory_order_acquire);
                if (!head)
                    return false;

                hp->hazard_.store(head, std::memory_order_acq_rel);
                //TODO:#StoreLoad brarrier
            }
            while (head != head_.load(std::memory_order_relaxed));
        }
        while (!head_.compare_exchange_strong(head, head->next_, std::memory_order_release, std::memory_order_relaxed));

        //take ownership of head pointer
        //auto deleter = [this](stack_node* node) { destroy_node(node); };
        //std::unique_ptr<stack_node, decltype(deleter)> head_owner(head, deleter);

        value = std::move(head->data_);

        gc_.release(hp);

        //retire(head);
        destroy_node(node);
        return true;
    }



    void push(value_type const& value)
    {
        stack_node* new_head = construct_node(value);

        new_head->next_ = head_.load(std::memory_order_relaxed);
        //release memory order - synchronizes with acquire load in try_pop
        while (!head_.compare_exchange_strong(new_head->next_, new_head, std::memory_order_release, std::memory_order_relaxed));
    }

    //void push(value_type&& value){}


private:
    bool try_pop_head(stack_node** result)
    {
        //acquire memory order garantees to read correct value of next_ pointer and data_
        stack_node* head = head_.load(std::memory_order_acquire);
        if (head && head_.compare_exchange_strong(head, head->next_, std::memory_order_relaxed, std::memory_order_relaxed))
        {
            *result = head;
            return true;
        }

        return false;
    }

    stack_node* construct_node(value_type const& value)
    {
        stack_node* node = alloc_.allocate(1);
        try
        {
            new (node) stack_node(value);
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
            node->~stack_node();
            alloc_.deallocate(node, 1);
        }
    }

private:
    std::atomic<stack_node*> head_;

    //allocator_type alloc_;
    node_allocator_type alloc_;

    //hazard pointer reclamaition strategy
    hazard_pointer_gc gc_;

    //noncopyable
    stack(stack const&);
    stack& operator=(stack const&);
};

}//namespace concurrent

#endif //CONCURRENT_STACK_H_