#ifndef CONCURRENT_STACK_H_
#define CONCURRENT_STACK_H_

#include <atomic>
#include <memory>

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


    //TODO:think how to implement exception safe stack::pop without try catch
    bool try_pop(value_type& value)
    {
        stack_node* head;

        //acquire load, granteed to read correct value of next_ pointer and data_
        //should be changed to memory_order_consume
        while (head = head_.load(std::memory_order_acquire))
        {
            if (head_.compare_exchange_strong(head, head->next_, std::memory_order_release, std::memory_order_relaxed))
                break;
        }

        if (!head)
            return false;


        //take ownership of head pointer
        auto deleter = [this](stack_node* node) { destroy_node(node); };
        std::unique_ptr<stack_node, decltype(deleter)> head_owner(head, deleter);

        value = std::move(head->data_);
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
        while (!head_.compare_exchange_strong(prev_head, new_head, std::memory_order_acq_rel, std::memory_order_acquire));
    }

    //void push(value_type&& value){}


private:
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

    //noncopyable
    stack(stack const&);
    stack& operator=(stack const&);
};

}//namespace concurrent

#endif //CONCURRENT_STACK_H_