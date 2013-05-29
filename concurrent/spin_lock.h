//empty file
#include <atomic>


namespace concurrent
{

class spin_lock
{
public:
    spin_lock() : spin_(0) {}

    void lock() {}
    void unlock() {}

private:
    bool try_lock() {}
private:
    std::atomic<int> spin_;
};
	
}//namespace concurrent


