#include "brick-hashset"
#include "brick-malloc"
#include "brick-unit"

#ifdef __divine__
static constexpr int size = 4;
static constexpr int threads = 2;
#else
static constexpr int size = 32 * 1024;
static constexpr int threads = 10;
#endif

struct big
{
    int v;
    std::array< int, 16 > padding;
    big( int v = 0 ) : v( v ) {}
    brq::hash64_t hash() const { return brq::hash( v ); }
    bool operator==( const big &o ) const { return v == o.v; }
    bool operator==( int o ) const { return v == o; }
    operator int() const { return v; }
};

template< template< typename, typename, int, typename > class hashset_t,
          typename value_t, typename alloc_t >
void test_sequential()
{
    using hashset = hashset_t< value_t, brq::impl::quick, 24, alloc_t >;

    brq::test_case( "insert_basic" ) = [=]
    {
        hashset set;

        ASSERT( !set.count( 1 ) );
        ASSERT( set.insert( 1 ).isnew() );
        ASSERT( set.count( 1 ) );

        unsigned count = 0;
        for ( unsigned i = 0; i != set.capacity(); ++i )
            if ( set.valueAt( i ) )
                ++count;

        ASSERT_EQ( count, 1u );
    };

    brq::test_case( "stress" ) = [=]
    {
        hashset set;

        for ( int i = 1; i < size; ++i ) {
            set.insert( i );
            ASSERT( set.count( i ) );
        }
        for ( int i = 1; i < size; ++i ) {
            ASSERT( set.count( i ) );
        }
    };

    if constexpr ( hashset::cell_t::can_tombstone() )
    {
        brq::test_case( "erase_basic" ) = [=]
        {
            hashset set;

            ASSERT( !set.count( 1 ) );
            ASSERT( set.insert( 1 ).isnew() );
            ASSERT( set.count( 1 ) );
            ASSERT( set.erase( 1 ) );
            ASSERT( !set.count( 1 ) );
            ASSERT( set.insert( 1 ).isnew() );
            ASSERT( set.count( 1 ) );
        };

        brq::test_case( "erase_many" ) = [=]
        {
            hashset set;

            for ( int i = 1; i < size; ++i )
            {
                set.insert( i );
                ASSERT( set.count( i ) );
                if ( i % 2 == 0 )
                {
                    set.erase( i );
                    ASSERT( !set.count( i ) );
                }
            }

            for ( int i = 1; i < size; ++i )
                ASSERT_EQ( set.count( i ), i % 2 );
        };
    }

    brq::test_case( "set" ) = [=]
    {
        hashset set;

        for ( int i = 1; i < size; ++i ) {
            ASSERT( !set.count( i ) );
        }

        for ( int i = 1; i < size; ++i ) {
            set.insert( i );
            ASSERT( set.count( i ) );
            ASSERT( !set.count( i + 1 ) );
        }

        for ( int i = 1; i < size; ++i ) {
            ASSERT( set.count( i ) );
        }

        for ( int i = size; i < 2 * size; ++i ) {
            ASSERT( !set.count( i ) );
        }
    };
}

template< typename hashset >
struct test_inserter
{
    hashset set;
    int from, to;
    bool overlap;

    void main()
    {
        for ( int i = from; i < to; ++i )
        {
            set.insert( i );
            ASSERT( !set.insert( i ).isnew() );
            if ( !overlap && i < to - 1 )
                ASSERT( !set.count( i + 1 ) );
        }
    }
};

template< typename inserter >
auto test_par( int f1, int t1, int f2, int t2 )
{
    brick::shmem::Thread< inserter > a, b( a );

    a.from = f1;
    a.to = t1;
    b.from = f2;
    b.to = t2;
    a.overlap = b.overlap = (t1 > f2);

    a.start();
    b.start();
    a.join();
    b.join();
    return a.set;
}

template< typename inserter >
auto test_multi( std::size_t count, int from, int to )
{
    brick::shmem::ThreadSet< inserter > arr;

    arr.resize( count );

    for ( std::size_t i = 0; i < count; ++i )
    {
        arr[ i ].from = from;
        arr[ i ].to = to;
        arr[ i ].overlap = true;
    }

    arr.start();
    arr.join();

    return arr[ 0 ].set;
}

template< template< typename, typename, int, typename > class hashset_t,
          typename value_t, typename alloc_t >
void test_parallel()
{
    using hashset = hashset_t< value_t, brq::impl::quick, 24, alloc_t >;
    using inserter = test_inserter< hashset >;

    brq::test_case( "insert_par" ) = [=]
    {
        inserter a;
        a.from = 1;
        a.to = size;
        a.overlap = false;
        a.main();

        for ( int i = 1; i < size; ++i )
            ASSERT( a.set.count( i ) );
    };

    brq::test_case( "multi" ) = [=]
    {
        auto set = test_multi< inserter >( threads, 1, size );

        for  ( int i = 1; i < size; ++i )
            ASSERT( set.count( i ) );

        int count = 0;
        std::set< int > s;

        for ( size_t i = 0; i != set.capacity(); ++i )
            if ( set.valueAt( i ) )
            {
                if ( s.find( set.valueAt( i ) ) == s.end() )
                    s.insert( set.valueAt( i ) );
                ++count;
            }

        ASSERT_EQ( count, size - 1 );
    };

    brq::test_case( "stress" ) = [=]
    {
        auto s = test_par< inserter >( 1, size / 2, size / 4, size );

        for ( int i = 1; i < size; ++i )
            ASSERT( s.count( i ) );
    };

    brq::test_case( "empty" ) = [=]
    {
        hashset set;

        for ( int i = 1; i < size; ++i )
            ASSERT( !set.count( i ) );
    };

    brq::test_case( "set" ) = [=]
    {
        auto set = test_par< inserter >( 1, size / 2, size / 2, size );

        for ( int i = 1; i < size; ++i )
            ASSERT_EQ( i, i * set.count( i ) );

        for ( int i = size; i < size * 2; ++i )
            ASSERT( !set.count( i ) );
    };
}

int main()
{
    using namespace brq;

    test_sequential< hash_set, int, std_bytealloc >();
    test_sequential< concurrent_hash_set, int, std_bytealloc >();
    test_parallel< concurrent_hash_set, int, std_bytealloc >();

    test_sequential< hash_set, int64_t, std_bytealloc >();
    test_sequential< concurrent_hash_set, int64_t, std_bytealloc >();
    test_parallel< concurrent_hash_set, int64_t, std_bytealloc >();

    test_sequential< hash_set, big, std_bytealloc >();
    test_sequential< concurrent_hash_set, big, std_bytealloc >();
    test_parallel< concurrent_hash_set, big, std_bytealloc >();

    test_sequential< hash_set, int, mm_bytealloc >();
    test_sequential< concurrent_hash_set, int, mm_bytealloc >();

}

#ifdef BRICK_BENCHMARK_REG

#include <brick-hlist.h>
#include <brick-benchmark.h>
#include <unordered_set>

#ifdef BRICKS_HAVE_TBB
#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_unordered_set.h>
#endif

namespace brick {
namespace b_hashset {

template< typename HS >
struct RandomThread : brick::shmem::Thread {
    HS *_set;
    typename HS::ThreadData td;
    int count, id;
    std::mt19937 rand;
    std::uniform_int_distribution<> dist;
    bool insert;
    int max;

    RandomThread() : insert( true ) {}

    void main() {
        rand.seed( id );
        auto set = _set->withTD( td );
        for ( int i = 0; i < count; ++i ) {
            int v = dist( rand );
            if ( max < std::numeric_limits< int >::max() ) {
                v = v % max;
                v = v * v + v + 41; /* spread out the values */
            }
            if ( insert )
                set.insert( v );
            else
                set.count( v );
        }
    };
};

namespace {

Axis axis_items( int min = 16, int max = 16 * 1024 ) {
    Axis a;
    a.type = Axis::Quantitative;
    a.name = "items";
    a.log = true;
    a.step = sqrt(sqrt(2));
    a.normalize = Axis::Div;
    a.unit = "k";
    a.unit_div =    1000;
    a.min = min * 1000;
    a.max = max * 1000;
    return a;
}

Axis axis_threads( int max = 16 ) {
    Axis a;
    a.type = Axis::Quantitative;
    a.name = "threads";
    a.normalize = Axis::Mult;
    a.unit = "";
    a.min = 1;
    a.max = max;
    a.step = 1;
    return a;
}

Axis axis_reserve( int max = 200, int step = 50 )
{
    Axis a;
    a.type = Axis::Quantitative;
    a.name = "reserve";
    a.unit = "%";
    a.min = 0;
    a.max = max;
    a.step = step;
    return a;
}

Axis axis_types( int count )
{
    Axis a;
    a.type = Axis::Qualitative;
    a.name = "type";
    a.unit = "";
    a.min = 0;
    a.max = count - 1;
    a.step = 1;
    return a;
}

}

template< typename T > struct TN {};
template< typename > struct _void { typedef void T; };

template< typename Ts >
struct Run : BenchmarkGroup
{
    template< typename, int Id >
    std::string render( int, hlist::not_preferred ) { return ""; }

    template< typename Tss = Ts, int Id = 0, typename = typename Tss::Head >
    std::string render( int id, hlist::preferred = hlist::preferred() )
    {
        if ( id == Id )
            return TN< typename Tss::Head >::n();
        return render< typename Tss::Tail, Id + 1 >( id, hlist::preferred() );
    }

    std::string describe() {
        std::string s;
        for ( int i = 0; i < int( Ts::length ); ++i )
            s += " type:" + render( i );
        return std::string( s, 1, s.size() );
    }

    template< template< typename > class, typename Self, int, typename, typename... Args >
    static void run( Self *, hlist::not_preferred, Args... ) {
        UNREACHABLE( "brick::b_hashset::Run fell off the cliff" );
    }

    template< template< typename > class RI, typename Self, int id,
              typename Tss, typename... Args >
    static auto run( Self *self, hlist::preferred, Args... args )
        -> typename _void< typename Tss::Head >::T
    {
        if ( self->type() == id ) {
            RI< typename Tss::Head > x( self, args... );
            self->reset(); // do not count the constructor
            x( self );
        } else
            run< RI, Self, id + 1, typename Tss::Tail, Args... >( self, hlist::preferred(), args... );
    }

    template< template< typename > class RI, typename Self, typename... Args >
    static void run( Self *self, Args... args ) {
        run< RI, Self, 0, Ts, Args... >( self, hlist::preferred(), args... );
    }

    int type() { return 0; } // default
};

template< int _threads, typename T >
struct ItemsVsReserve : Run< hlist::TypeList< T > >
{
    ItemsVsReserve() {
        this->x = axis_items();
        this->y = axis_reserve();
    }

    std::string fixed() {
        std::stringstream s;
        s << "threads:" << _threads;
        return s.str();
    }

    int threads() { return _threads; }
    int items() { return this->p; }
    double reserve() { return this->q / 100; }
    double normal() { return _threads; }
};

template< int _max_threads, int _reserve, typename T >
struct ItemsVsThreads : Run< hlist::TypeList< T > >
{
    ItemsVsThreads() {
        this->x = axis_items();
        this->y = axis_threads( _max_threads );
    }

    std::string fixed() {
        std::stringstream s;
        s << "reserve:" << _reserve;
        return s.str();
    }

    int threads() { return this->q; }
    int items() { return this->p; }
    double reserve() { return _reserve / 100.0; }
};

template< int _items, typename T >
struct ThreadsVsReserve : Run< hlist::TypeList< T > >
{
    ThreadsVsReserve() {
        this->x = axis_threads();
        this->y = axis_reserve();
    }

    std::string fixed() {
        std::stringstream s;
        s << "items:" << _items << "k";
        return s.str();
    }

    int threads() { return this->p; }
    int reserve() { return this->q; }
    int items() { return _items * 1000; }
};

template< int _threads, int _reserve, typename... Ts >
struct ItemsVsTypes : Run< hlist::TypeList< Ts... > >
{
    ItemsVsTypes() {
        this->x = axis_items();
        this->y = axis_types( sizeof...( Ts ) );
        this->y._render = [this]( int i ) {
            return this->render( i );
        };
    }

    std::string fixed() {
        std::stringstream s;
        s << "threads:" << _threads << " reserve:" << _reserve;
        return s.str();
    }

    int threads() { return _threads; }
    double reserve() { return _reserve / 100.0; }
    int items() { return this->p; }
    int type() { return this->q; }
    double normal() { return _threads; }
};

template< int _items, int _reserve, int _threads, typename... Ts >
struct ThreadsVsTypes : Run< hlist::TypeList< Ts... > >
{
    ThreadsVsTypes() {
        this->x = axis_threads( _threads );
        this->y = axis_types( sizeof...( Ts ) );
        this->y._render = [this]( int i ) {
            return this->render( i );
        };
    }

    std::string fixed() {
        std::stringstream s;
        s << "items:" << _items << "k reserve:" << _reserve;
        return s.str();
    }

    int threads() { return this->p; }
    double reserve() { return _reserve / 100.0; }
    int items() { return _items * 1000; }
    int type() { return this->q; }
    double normal() { return 1.0 / items(); }
};

template< typename T >
struct RandomInsert {
    bool insert;
    int max;
    using HS = typename T::template HashTable< int >;
    HS t;

    template< typename BG >
    RandomInsert( BG *bg, int max = std::numeric_limits< int >::max() )
        : insert( true ), max( max )
    {
        if ( bg->reserve() > 0 )
            t.reserve( bg->items() * bg->reserve() );
    }

    template< typename BG >
    void operator()( BG *bg )
    {
        RandomThread< HS > *ri = new RandomThread< HS >[ bg->threads() ];

        for ( int i = 0; i < bg->threads(); ++i ) {
            ri[i].id = i;
            ri[i].insert = insert;
            ri[i].max = max;
            ri[i].count = bg->items() / bg->threads();
            ri[i]._set = &t;
        }

        for ( int i = 0; i < bg->threads(); ++i )
            ri[i].start();
        for ( int i = 0; i < bg->threads(); ++i )
            ri[i].join();
    }
};

template< typename T >
struct RandomLookup : RandomInsert< T > {

    template< typename BG >
    RandomLookup( BG *bg, int ins_max, int look_max )
        : RandomInsert< T >( bg, ins_max )
    {
        (*this)( bg );
        this->max = look_max;
        this->insert = false;
    }
};

template< typename Param >
struct Bench : Param
{
    std::string describe() {
        return "category:hashset " + Param::describe() + " " +
            Param::fixed() + " " + this->describe_axes();
    }

    BENCHMARK(random_insert_1x) {
        this->template run< RandomInsert >( this );
    }

    BENCHMARK(random_insert_2x) {
        this->template run< RandomInsert >( this, this->items() / 2 );
    }

    BENCHMARK(random_insert_4x) {
        this->template run< RandomInsert >( this, this->items() / 4 );
    }

    BENCHMARK(random_lookup_100) {
        this->template run< RandomInsert >( this );
    }

    BENCHMARK(random_lookup_50) {
        this->template run< RandomLookup >(
            this, this->items() / 2, this->items() );
    }

    BENCHMARK(random_lookup_25) {
        this->template run< RandomLookup >(
            this, this->items() / 4, this->items() );
    }
};

template< template< typename > class C >
struct wrap_hashset {
    template< typename T > using HashTable = C< T >;
};

template< template< typename > class C >
struct wrap_set {
    template< typename T >
    struct HashTable {
        C< T > *t;
        struct ThreadData {};
        HashTable< T > withTD( ThreadData & ) { return *this; }
        void reserve( int s ) { t->rehash( s ); }
        void insert( T i ) { t->insert( i ); }
        int count( T i ) { return t->count( i ); }
        HashTable() : t( new C< T > ) {}
    };
};

struct empty {};

template< template< typename > class C >
struct wrap_map {
    template< typename T >
    struct HashTable : wrap_set< C >::template HashTable< T >
    {
        template< typename TD >
        HashTable< T > &withTD( TD & ) { return *this; }
        void insert( int v ) {
            this->t->insert( std::make_pair( v, empty() ) );
        }
    };
};

template< typename T >
using unordered_set = std::unordered_set< T >;

using A = wrap_set< unordered_set >;
using B = wrap_hashset< CS >;
using C = wrap_hashset< FS >;
using D = wrap_hashset< PS >;

template<> struct TN< A > { static const char *n() { return "std"; } };
template<> struct TN< B > { static const char *n() { return "scs"; } };
template<> struct TN< C > { static const char *n() { return "sfs"; } };
template<> struct TN< D > { static const char *n() { return "ccs"; } };
template<> struct TN< E > { static const char *n() { return "cfs"; } };

#define FOR_SEQ(M) M(A) M(B) M(C)
#define SEQ A, B, C

#ifdef BRICKS_HAVE_TBB
#define FOR_PAR(M) M(D) M(E) M(F) M(G)
#define PAR D, E, F, G

template< typename T > using cus = tbb::concurrent_unordered_set< T >;
template< typename T > using chm = tbb::concurrent_hash_map< T, empty >;

using F = wrap_set< cus >;
using G = wrap_map< chm >;

template<> struct TN< F > { static const char *n() { return "cus"; } };
template<> struct TN< G > { static const char *n() { return "chm"; } };

#else
#define FOR_PAR(M) M(D) M(E)
#define PAR D, E
#endif

#define TvT(N) \
    template struct Bench< ThreadsVsTypes< N, 50, 4, PAR > >;

TvT(1024)
TvT(16 * 1024)

#define IvTh_PAR(T) \
  template struct Bench< ItemsVsThreads< 4, 0, T > >;

template struct Bench< ItemsVsTypes< 1, 0, SEQ, PAR > >;
template struct Bench< ItemsVsTypes< 2, 0, PAR > >;
template struct Bench< ItemsVsTypes< 4, 0, PAR > >;

#define IvR_SEQ(T) \
  template struct Bench< ItemsVsReserve< 1, T > >;
#define IvR_PAR(T) \
  template struct Bench< ItemsVsReserve< 1, T > >; \
  template struct Bench< ItemsVsReserve< 2, T > >; \
  template struct Bench< ItemsVsReserve< 4, T > >;

FOR_PAR(IvTh_PAR)

FOR_SEQ(IvR_SEQ)
FOR_PAR(IvR_PAR)

#undef FOR_SEQ
#undef FOR_PAR
#undef SEQ
#undef PAR
#undef IvT_PAR
#undef IvR_SEQ
#undef IvR_PAR


}
}

#endif // benchmarks
