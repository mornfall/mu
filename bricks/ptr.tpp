#include "brick-ptr"
#include "brick-unit"

template< bool atomic >
struct ref_test : brq::refcount_base< uint16_t, atomic >
{
    int &cnt;
    ref_test( int &cnt ) : cnt( cnt ) { ++cnt; }
    ~ref_test() { --cnt; }
};

template< bool atomic_cnt >
struct test_obj : brq::refcount_base< uint16_t, atomic_cnt > {};

template< bool atomic_ptr, bool atomic_cnt >
void test_ptr()
{
    using obj = test_obj< atomic_cnt >;

    brq::test_case( "simple" ) = [=]
    {
        auto a = brq::make_refcount_opt< obj, atomic_ptr >();
        ASSERT_EQ( a->_refcount, 1 );
    };

    brq::test_case( "dtor" ) = [=]
    {
        auto a = brq::make_refcount_opt< obj, atomic_ptr >();
        ASSERT_EQ( a->_refcount, 1 );
        {
            auto b = a;
            ASSERT_EQ( a->_refcount, 2 );
        }
        ASSERT_EQ( a->_refcount, 1 );
    };

    brq::test_case( "assign" ) = [=]
    {
        auto a = brq::make_refcount_opt< obj, atomic_ptr >();
        decltype( a ) b;
        ASSERT_EQ( a->_refcount, 1 );
        b = a;
        ASSERT_EQ( a->_refcount, 2 );
        *&b = b;
        ASSERT_EQ( a->_refcount, 2 );
    };

    brq::test_case( "destroy" ) = [=]
    {
        int objs = 0;
        {
            auto a = brq::make_refcount_opt< ref_test< atomic_cnt >, atomic_ptr >( objs );
            ASSERT_EQ( objs, 1 );
        }
        ASSERT_EQ( objs, 0 );
    };
}

int main()
{
    test_ptr< true, true >();
    test_ptr< false, true >();
    test_ptr< true, false >();
    test_ptr< false, false >();
}
