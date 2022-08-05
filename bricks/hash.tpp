#include "brick-hash"
#include "brick-unit"

template< bool strict >
void test_hash()
{
    const char *str = "abcdefghijklmnopABCDEFGHIJKLMNOPabcdefghijklmnopABCDEFGHIJKLMNOP_";
    const int BUFSIZE = 512;

    brq::test_case( "bytewise" ) = [=]
    {
        for ( unsigned len = 0; len < strlen( str ); ++len )
        {
            brq::hash_state s1( len ), s2( len );
            s1.update_aligned< strict >( reinterpret_cast< const uint8_t * >( str ), len );
            for ( unsigned i = 0; i < len; ++i )
                s2.update( str[i] );
            ASSERT_EQ( s1.hash(), s2.hash() );
        }

    };

    brq::test_case( "piecewise_unaligned" ) = [=]
    {
        for ( unsigned len = 0; len < strlen( str ); ++len )
        {
            brq::hash_state s1( len ), s2( len );
            s1.update_aligned< strict >( reinterpret_cast< const uint8_t * >( str ), len );

            for ( unsigned i = 0; i < len; ++i )
            {
                if ( len - i > 4 && i % 3 == 0 )
                    s2.update( *reinterpret_cast< const uint32_t * >( str + i ) ), i += 3;
                else
                    s2.update( str[i] );
            }

            ASSERT_EQ( s1.hash(), s2.hash() );
        }

    };

    /* following two testcases are derived from Bob Jenkins's public domain code */

    brq::test_case( "alignment" ) = [=]
    {
        char buf[ BUFSIZE ];
        uint64_t hash[ 8 ];

        for ( int i = 0; i < BUFSIZE - 16; ++i )
        {
            for ( int j = 0; j < 8; ++j )
            {
                buf[ j ] = char( i + j );
                for ( int k = 1; k <= i; ++k )
                    buf[ j + k ] = k;
                buf[ j + i + 1 ] = char( i + j );
                hash[j] = brq::hash( reinterpret_cast< const uint8_t * >( buf + j + 1 ), i, 0 );
            }

            for ( int j = 1; j < 8; ++ j )
                ASSERT_EQ( hash[0], hash[j] );
        }
    };

    brq::test_case( "pieces" ) = [=]
    {
        uint8_t buf[ BUFSIZE ];

        for ( int i = 0; i < BUFSIZE; ++i )
            buf[ i ] = i;

        for ( int i = 0; i < BUFSIZE; ++i )
        {
            uint64_t a, c, seed = 1;

            // all as one call
            a = brq::hash( buf, i, seed );

            brq::hash_state state( seed );
            state.update_aligned< strict >( buf, i );
            c = state.hash();

            ASSERT_EQ( a, c );

            // all possible two consecutive pieces
            for ( int j = 0; j < i; ++ j )
            {
                brq::hash_state state( seed );
                state.update( buf, j );
                state.update( buf + j, i - j );
                c = state.hash();
                TRACE( j, i - j );
                ASSERT_EQ( a, c );
            }
        }
    };
}

int main()
{
    test_hash< false >();
    test_hash< true >();
}
