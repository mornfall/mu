#include "brick-compact"
#include "brick-unit"

template< template< typename > class sort_ >
void test_map()
{
    using sort = sort_< brq::less_map >;

    brq::test_case( "simple" ) = [=]
    {
        brq::array_map< std::string, std::string, sort > x;

        ASSERT( x.insert( std::make_pair( "bKey", "bVal" ) ).second );
        ASSERT( !x.insert( std::make_pair( "bKey", "bVal" ) ).second );
        ASSERT_EQ( x.size(), 1 );
        ASSERT_EQ( x.find( "bKey" )->second, "bVal" );

        ASSERT( x.emplace( "aKey", "aVal" ).second );
        ASSERT( !x.emplace( "aKey", "aVal" ).second );
        ASSERT_EQ( x.size(), 2 );
        ASSERT_EQ( x.find( "aKey" )->second, "aVal" );
        ASSERT_EQ( x.find( "bKey" )->second, "bVal" );

        x[ "cKey" ] = "cVal";
        ASSERT_EQ( x.size(), 3 );
        ASSERT_EQ( x.find( "aKey" )->second, "aVal" );
        ASSERT_EQ( x.find( "bKey" )->second, "bVal" );
        ASSERT_EQ( x.find( "cKey" )->second, "cVal" );

        x.erase( "bKey" );
        ASSERT_EQ( x.find( "aKey" )->second, "aVal" );
        ASSERT_EQ( x.find( "cKey" )->second, "cVal" );

        ASSERT_EQ( x.at( "aKey" ), "aVal" );
        ASSERT_EQ( x.at( "cKey" ), "cVal" );
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
