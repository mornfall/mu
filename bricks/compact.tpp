#include "brick-compact"
#include "brick-unit"

template< template< typename > class sort_ >
void test_map()
{
    using sort = sort_< brq::less_map >;

    auto test_seq = []( auto &map, auto keys, auto insert )
    {
        int i = map.size();

        for ( auto bk : keys )
        {
            std::string val = "val_" + std::to_string( i );
            auto [ it_0, new_0 ] = insert( map, bk, val );
            ASSERT( new_0 );
            ASSERT_EQ( it_0->first, bk );
            ASSERT_EQ( it_0->second, val );

            auto [ it_1, new_1 ] = insert( map, bk, bk );
            ASSERT( !new_1 );
            ASSERT_EQ( it_1->first, bk );
            ASSERT_EQ( it_1->second, val );

            ASSERT_EQ( map.size(), ++i );
            ASSERT_EQ( map.find( bk )->second, val );
        }
    };

    auto do_insert  = []( auto &m, auto... args ) { return m.insert( std::pair{ args... } ); };
    auto do_emplace = []( auto &m, auto... args ) { return m.emplace( args... ); };

    auto test_insert  = [&]( auto &map, auto keys ) { test_seq( map, keys, do_insert ); };
    auto test_emplace = [&]( auto &map, auto keys ) { test_seq( map, keys, do_emplace ); };

    brq::test_case( "simple" ) = [=]
    {
        brq::array_map< std::string, std::string, sort > x;
        ASSERT_EQ( x.size(), 0 );

        test_insert ( x, std::vector{ "a_key1", "a_key3", "a_key2", "a_key0" } );
        test_emplace( x, std::vector{ "b_key1", "b_key3", "b_key2", "b_key0" } );
        x.clear();
        test_emplace( x, std::vector{ "a_key1", "a_key3", "a_key2", "a_key0" } );
        test_insert ( x, std::vector{ "b_key1", "b_key3", "b_key2", "b_key0" } );

        ASSERT_EQ( x.size(), 8 );
        ASSERT_EQ( x.find( "a_key1" )->second, "val_0" );
        ASSERT_EQ( x.find( "b_key3" )->second, "val_5" );

        x[ "c_key0" ] = "val_0";
        ASSERT_EQ( x.size(), 9 );
        ASSERT_EQ( x.find( "a_key0" )->second, "val_3" );
        ASSERT_EQ( x.find( "b_key0" )->second, "val_7" );
        ASSERT_EQ( x.find( "c_key0" )->second, "val_0" );

        x.erase( "b_key2" );
        ASSERT_EQ( x.find( "a_key2" )->second, "val_2" );
        ASSERT_EQ( x.find( "c_key0" )->second, "val_0" );

        ASSERT_EQ( x.at( "a_key0" ), "val_3" );
        ASSERT_EQ( x.at( "c_key0" ), "val_0" );
    };

    brq::test_case( "out_of_range" ) = [=]
    {
        brq::array_map< int, std::string, sort > am;
        am[ 1 ] = "aKey";
        try {
            am.at( 2 );
            ASSERT( false );
        } catch ( std::out_of_range & ) { }
    };

    brq::test_case( "comparison" ) = [=]
    {
        brq::array_map< int, int, sort > m;
        m.emplace( 1, 1 );
        m.emplace( 2, 1 );

        auto m2 = m;
        ASSERT( m == m2 );
        ASSERT( !(m != m2) );

        ASSERT( m <= m2 );
        ASSERT( m2 <= m );

        ASSERT( !(m < m2) );
        ASSERT( !(m2 < m) );

        m2.emplace( 3, 1 );
        ASSERT( m != m2 );
        ASSERT( m <= m2 );
        ASSERT( m < m2 );

        m2.erase( m2.find( 3 ) );
        m2[ 2 ] = 2;
        ASSERT( m != m2 );
        ASSERT( m <= m2 );
        ASSERT( m < m2 );
    };
}

int main()
{
    test_map< brq::std_sort >();
    test_map< brq::insert_sort >();

    using m = brq::dense_map< int, char >;

    brq::test_case( "dense_map basic" ) = [=]
    {
        m map;
        map[ 3 ] = 'c';
        ASSERT_EQ( map.count( 3 ), 1 );
        ASSERT_EQ( map[ 3 ], 'c' );
        map[ 1 ] = 'a';
        ASSERT_EQ( map.count( 1 ), 1 );
        ASSERT_EQ( map[ 1 ], 'a' );
    };
}
