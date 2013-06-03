#ifndef CONCURRENT_TESTS_API_H_
#define CONCURRENT_TESTS_API_H_
#include <functional>
#include <vector>


typedef void (*test_func)();

class test_api
{
private:
    typedef std::vector< test_func >                    tests_vector;

    static tests_vector& tests()
    {
        static tests_vector functors;
        return functors;
    }

public:
    static void register_test(test_func test)
    {
        tests().push_back(test);
    }


    static void run()
    {
        for (auto it = std::begin(tests()); it != std::end(tests()); ++it)
        {
            (*it)();
        }
    }

};

template<test_func test>
class static_test_register
{
public:
    static_test_register()
    {
        test_api::register_test(test);
    }
};


#define REGISTER_TEST(func) namespace { static static_test_register<func> t; }

#endif //CONCURRENT_TESTS_API_H_