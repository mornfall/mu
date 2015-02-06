// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * Utilities and data structures for shared-memory parallelism. Includes:
 * - shared memory, lock-free first-in/first-out queue (one reader + one writer)
 * - a spinlock
 * - approximate counter (share a counter between threads without contention)
 * - a weakened atomic type (like std::atomic)
 * - a derivable wrapper around std::thread
 */

/*
 * (c) 2008, 2012 Petr Ročkai <me@mornfall.net>
 * (c) 2011 Tomáš Janoušek <tomi@nomi.cz>
 * (c) 2014 Vladimír Štill <xstill@fi.muni.cz>
 */

/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include <brick-assert.h>
#include <deque>

#if __cplusplus >= 201103L
#include <mutex>
#include <atomic>
#include <thread>
#include <stdexcept>
#include <mutex>
#endif

#ifndef BRICK_SHMEM_H
#define BRICK_SHMEM_H

#ifndef BRICKS_CACHELINE
#define BRICKS_CACHELINE 64
#endif

namespace brick {
namespace shmem {

#if __cplusplus >= 201103L

struct Thread {
    std::unique_ptr< std::thread > _thread;
    std::atomic< bool > _interrupted;
    virtual void main() = 0;

    Thread() : _interrupted( false ) {}
    Thread( const Thread &other ) : _interrupted( false ) {
        if( other._thread )
            throw std::logic_error( "cannot copy running thread" );
    }
    Thread( Thread &&other ) :
        _thread( std::move( other._thread ) ),
        _interrupted( other.interrupted() )
    {}

    ~Thread() { stop(); }

    Thread &operator=( const Thread &other ) {
        if ( _thread )
            throw std::logic_error( "cannot overwrite running thread" );
        if ( other._thread )
            throw std::logic_error( "cannot copy running thread" );
        _interrupted.store( other.interrupted(), std::memory_order_relaxed );
        return *this;
    }

    Thread &operator=( Thread &&other ) {
        if ( _thread )
            throw std::logic_error( "cannot overwrite running thread" );
        _thread.swap( other._thread );
        _interrupted.store( other.interrupted(), std::memory_order_relaxed );
        return *this;
    }

#ifdef __divine__
    void start() __attribute__((noinline)) {
        __divine_interrupt_mask();
#else
    void start() {
#endif
        _interrupted.store( false, std::memory_order_relaxed );
        _thread.reset( new std::thread( [this]() { this->main(); } ) );
    }

    // stop must be idempotent
    void stop() {
        interrupt();
        if ( _thread && _thread->joinable() )
            join();
    }

    void join() {
        if ( _thread ) {
            _thread->join();
            _thread.reset();
        }
    }

    void detach() {
        if ( _thread ) {
            _thread->detach();
            _thread.reset();
        }
    }

    bool interrupted() const {
        return _interrupted.load( std::memory_order_relaxed );
    }

    void interrupt() {
        _interrupted.store( true, std::memory_order_relaxed );
    }
};

/**
 * A spinlock implementation.
 *
 * One has to wonder why this is missing from the C++0x stdlib.
 */
struct SpinLock {
    std::atomic_flag b;

    SpinLock() : b( ATOMIC_FLAG_INIT ) {}

    void lock() {
        while( b.test_and_set() );
    }

    void unlock() {
        b.clear();
    }

    SpinLock( const SpinLock & ) = delete;
    SpinLock &operator=( const SpinLock & ) = delete;
};

/**
 * Termination detection implemented as a shared counter of open (not yet
 * processed) states. This appears to be fast because the shared counter is
 * modified very rarely -- its incremented in large steps for thousands of
 * states in advance and then adjusted down to its actual value only if the
 * queue gets empty.
 *
 * Shared counter - Σ local of all threads = actual open count.
 * local ≥ 0, hence shared is an overapproximation of the actual open count.
 * This implies partial correctness.
 *
 * Termination follows from proper calls to sync().
 */
struct ApproximateCounter {
    enum { step = 100000 };

    struct Shared {
        std::atomic< intptr_t > counter;
        Shared() : counter( 0 ) {}

        Shared( const Shared& ) = delete;
    };

    Shared &shared;
    intptr_t local;

    ApproximateCounter( Shared &s ) : shared( s ), local( 0 ) {}
    ~ApproximateCounter() { sync(); }

    void sync() {
        intptr_t value = shared.counter;

        while ( local > 0 ) {
            if ( value >= local ) {
                if ( shared.counter.compare_exchange_weak( value, value - local ) )
                    local = 0;
            } else {
                if ( shared.counter.compare_exchange_weak( value, 0 ) )
                    local = 0;
            }
        }
    }

    ApproximateCounter& operator++() {
        if ( local == 0 ) {
            shared.counter += step;
            local = step;
        }

        --local;

        return *this;
    }

    ApproximateCounter &operator--() {
        ++local;
        return *this;
    }

    // NB. sync() must be called manually as this method is called too often
    bool isZero() {
        return shared.counter == 0;
    }

    void reset() { shared.counter = 0; }

    ApproximateCounter( const ApproximateCounter &a )
        : shared( a.shared ), local( a.local )
    {}
    ApproximateCounter operator=( const ApproximateCounter & ) = delete;
};

struct StartDetector {

    struct Shared {
        std::atomic< unsigned short > counter;

        Shared() : counter( 0 ) {}
        Shared( Shared & ) = delete;
    };

    Shared &shared;

    StartDetector( Shared &s ) : shared( s ) {}
    StartDetector( const StartDetector &s ) : shared( s.shared ) {}

    void waitForAll( unsigned short peers ) {
        if ( ++shared.counter == peers )
            shared.counter = 0;

        while ( shared.counter );
    }

};

/*
 * Simple wrapper around atomic with weakened memory orders.
 *
 * The WeakAtomic users consume memory order for reading and release MO for
 * writing, which should assure atomicity and consistency of given variable,
 * however it does not assure consistence of other variables written before
 * given atomic location.  Read-modify-write operations use
 * memory_order_acq_rel.
 */

namespace _impl {

template< typename Self, typename T >
struct WeakAtomicIntegral {

    T operator |=( T val ) {
        return self()._data.fetch_or( val, std::memory_order_acq_rel ) | val;
    }

    T operator &=( T val ) {
        return self()._data.fetch_and( val, std::memory_order_acq_rel ) & val;
    }

    Self &self() { return *static_cast< Self * >( this ); }
};

struct Empty { };

}

template< typename T >
struct WeakAtomic : std::conditional< std::is_integral< T >::value && !std::is_same< T, bool >::value,
                      _impl::WeakAtomicIntegral< WeakAtomic< T >, T >,
                      _impl::Empty >::type
{
    WeakAtomic( T x ) : _data( x ) { }
    WeakAtomic() = default;

    operator T() const { return _data.load( std::memory_order_consume ); }
    T operator=( T val ) {
        _data.store( val, std::memory_order_release );
        return val;
    }

  private:
    std::atomic< T > _data;
    friend struct _impl::WeakAtomicIntegral< WeakAtomic< T >, T >;
};

#endif

#ifndef __divine__
template< typename T >
constexpr int defaultNodeSize() {
    return (32 * 4096 - BRICKS_CACHELINE - sizeof( void* )) / sizeof( T );
}
#else
template< typename T >
constexpr int defaultNodeSize() { return 3; }
#endif

/*
 * A simple queue (First-In, First-Out). Concurrent access to the ends of the
 * queue is supported -- a thread may write to the queue while another is
 * reading. Concurrent access to a single end is, however, not supported.
 *
 * The NodeSize parameter defines a size of single block of objects. By
 * default, we make the node a page-sized object -- this seems to work well in
 * practice. We rely on the allocator to align the allocated blocks reasonably
 * to give good cache usage.
 */

template< typename T, int NodeSize = defaultNodeSize< T >() >
struct Fifo {
protected:
    // the Node layout puts read and write counters far apart to avoid
    // them sharing a cache line, since they are always written from
    // different threads
    struct Node {
        T *read               __attribute__((__aligned__(BRICKS_CACHELINE)));
        T buffer[ NodeSize ]  __attribute__((__aligned__(BRICKS_CACHELINE)));
        T * volatile write;
        Node *next;
        Node() {
            read = write = buffer;
            next = 0;
        }
    };

    // pad the fifo object to ensure that head/tail pointers never
    // share a cache line with anyone else
    Node *head            __attribute__((__aligned__(BRICKS_CACHELINE)));
    Node * volatile tail  __attribute__((__aligned__(BRICKS_CACHELINE)));

public:
    Fifo() {
        head = tail = new Node();
        ASSERT( empty() );
    }

    // copying a fifo is not allowed
    Fifo( const Fifo & ) {
        head = tail = new Node();
        ASSERT( empty() );
    }

    ~Fifo() {
        while ( head != tail ) {
            Node *next = head->next;
            ASSERT( next != 0 );
            delete head;
            head = next;
        }
        delete head;
    }

    void push( const T&x ) {
        Node *t;
        if ( tail->write == tail->buffer + NodeSize )
            t = new Node();
        else
            t = tail;

        *t->write = x;
        ++ t->write;
        __sync_synchronize();

        if ( tail != t ) {
            tail->next = t;
            __sync_synchronize();
            tail = t;
        }
    }

    bool empty() {
        return head == tail && head->read >= head->write;
    }

    int size() {
    	int size = 0;
    	Node *n = head;
    	do {
            size += n->write - n->read;
            n = n->next;
        } while (n);
        return size;
    }

    void dropHead() {
        Node *old = head;
        head = head->next;
        ASSERT( head );
        delete old;
    }

    void pop() {
        ASSERT( !empty() );
        ++ head->read;
        if ( head->read == head->buffer + NodeSize ) {
            if ( head != tail ) {
                dropHead();
            }
        }
        // the following can happen when head->next is 0 even though head->read
        // has reached NodeSize, *and* no front() has been called in the meantime
        if ( head != tail && head->read > head->buffer + NodeSize ) {
            dropHead();
            pop();
        }
    }

    T &front( bool wait = false ) {
        while ( wait && empty() ) ;
        ASSERT( head );
        ASSERT( !empty() );
        // last pop could have left us with empty queue exactly at an
        // edge of a block, which leaves head->read == NodeSize
        if ( head->read == head->buffer + NodeSize ) {
            dropHead();
        }
        return *head->read;
    }
};

/*
 * A very simple spinlock-protected queue based on std::deque.
 */

template < typename T >
struct LockedQueue {
    typedef brick::shmem::SpinLock Mutex;
    Mutex m;
    brick::shmem::WeakAtomic< bool > _empty;
    std::deque< T > q;
    using element = T;

    LockedQueue( void ) : _empty( true ) {}

    bool empty() const { return _empty; }

    void push( const T &x ) {
        std::lock_guard< Mutex > lk( m );
        q.push_back( x );
        _empty = false;
    }

    void push( T &&x ) {
        std::lock_guard< Mutex > lk( m );
        q.push_back( std::move( x ) );
        _empty = false;
    }

    /**
     * Pops a whole chunk, to be processed by one thread as a whole.
     */
    T pop() {
        T ret = T();

        /* Prevent threads from contending for a lock if the queue is empty. */
        if ( empty() )
            return ret;

        std::lock_guard< Mutex > lk( m );

        if ( q.empty() )
            return ret;

        ret = std::move( q.front() );
        q.pop_front();

        if ( q.empty() )
            _empty = true;

        return ret;
    }

    void clear() {
        std::lock_guard< Mutex > guard{ m };
        q.clear();
        _empty = true;
    }

    LockedQueue( const LockedQueue & ) = delete;
    LockedQueue &operator=( const LockedQueue & ) = delete;
};

}
}

#if __cplusplus >= 201103L

#include <unistd.h> // alarm
#include <vector>

namespace brick_test {
namespace shmem {

using namespace ::brick::shmem;

struct FifoTest {
    template< typename T >
    struct Checker : Thread
    {
        Fifo< T > fifo;
        int terminate;
        int n;

        void main()
        {
            std::vector< int > x;
            x.resize( n );
            for ( int i = 0; i < n; ++i )
                x[ i ] = 0;

            while (true) {
                while ( !fifo.empty() ) {
                    int i = fifo.front();
                    ASSERT_EQ( x[i % n], i / n );
                    ++ x[ i % n ];
                    fifo.pop();
                }
                if ( terminate > 1 )
                    break;
                if ( terminate )
                    ++terminate;
            }
            terminate = 0;
            for ( int i = 0; i < n; ++i )
                ASSERT_EQ( x[ i ], 128*1024 );
        }

        Checker( int _n = 1 ) : terminate( 0 ), n( _n ) {}
    };

    TEST(stress) {
        Checker< int > c;
        for ( int j = 0; j < 5; ++j ) {
            c.start();
            for( int i = 0; i < 128 * 1024; ++i )
                c.fifo.push( i );
            c.terminate = true;
            c.join();
        }
    }
};

struct StartEnd {
    static const int peers = 12;

    struct DetectorWorker : Thread {

        StartDetector detector;

        DetectorWorker( StartDetector::Shared &sh ) :
            detector( sh )
        {}

        void main() {
            detector.waitForAll( peers );
        }
    };

    TEST(startDetector) {// this test must finish
        StartDetector::Shared sh;
        std::vector< DetectorWorker > threads{ peers, DetectorWorker{ sh } };

#if (defined( __unix ) || defined( POSIX )) && !defined( __divine__ ) // hm
        alarm( 1 );
#endif

        for ( int i = 0; i != 4; ++i ) {
            for ( auto &w : threads )
                w.start();
            for ( auto &w : threads )
                w.join();
            ASSERT_EQ( sh.counter.load(), 0 );
        }
    }

    struct CounterWorker : Thread {
        StartDetector detector;
        ApproximateCounter counter;
        std::atomic< int > &queue;
        std::atomic< bool > &interrupted;
        int produce;
        int consume;

        template< typename D, typename C >
        CounterWorker( D &d, C &c, std::atomic< int > &q, std::atomic< bool > &i ) :
            detector( d ),
            counter( c ),
            queue( q ),
            interrupted( i ),
            produce( 0 ),
            consume( 0 )
        {}

        void main() {
            detector.waitForAll( peers );
            while ( !counter.isZero() ) {
                ASSERT_LEQ( 0, queue.load() );
                if ( queue == 0 ) {
                    counter.sync();
                    continue;
                }
                if ( produce ) {
                    --produce;
                    ++queue;
                    ++counter;
                }
                if ( consume ) {
                    int v = queue;
                    if ( v == 0 || !queue.compare_exchange_strong( v, v - 1 ) )
                        continue;
                    --consume;
                    --counter;
                }
                if ( interrupted )
                    break;
            }
        }
    };

    void process( bool terminateEarly ) {
        StartDetector::Shared detectorShared;
        ApproximateCounter::Shared counterShared;
        std::atomic< bool > interrupted( false );

        // queueInitials
        std::atomic< int > queue{ 1 };
        ApproximateCounter c( counterShared );
        ++c;
        c.sync();

        std::vector< CounterWorker > threads{ peers,
            CounterWorker{ detectorShared, counterShared, queue, interrupted } };

#if (defined( __unix ) || defined( POSIX )) && !defined( __divine__ ) // hm
        alarm( 5 );
#endif

        // set consume and produce limits to each worker
        int i = 1;
        for ( auto &w : threads ) {
            w.produce = i;
            // let last worker consume the rest of produced values
            w.consume = peers - i;
            if ( w.consume == 0 )
                w.consume = peers + 1;// also initials
            ++i;
        }

        for ( auto &w : threads )
            w.start();

        if ( terminateEarly ) {
            interrupted = true;
            counterShared.counter = 0;
            queue = 0;
        }

        for ( auto &w : threads )
            w.join();

        if ( !terminateEarly ) { // do not check on early termination
            ASSERT_EQ( queue.load(), 0 );
            ASSERT_EQ( counterShared.counter.load(), 0 );
        }
    }

    TEST(approximateCounterProcessAll) {
        process( false );
    };

    TEST(approximateCounterTerminateEarly) {
        process( true );
    }
};

}
}

#ifdef BRICK_BENCHMARK_REG

#ifdef BRICKS_HAVE_TBB
#include <tbb/concurrent_queue.h>
#endif

#include <random>
#include <brick-benchmark.h>

namespace brick_test {
namespace shmem {

template< typename T >
struct Naive {
    std::deque< T > q;
    std::mutex m;

    void push( T x ) {
        std::lock_guard< std::mutex > __l( m );
        q.push_back( x );
    }

    void pop() {
        std::lock_guard< std::mutex > __l( m );
        q.pop_front();
    }

    T &front() {
        std::lock_guard< std::mutex > __l( m );
        return q.front();
    }

    bool empty() {
        std::lock_guard< std::mutex > __l( m );
        return q.empty();
    }
};

template< typename T, int size = 512 >
struct Ring {
    volatile int reader;
    T q[ size ];
    volatile int writer;

    void push( T x ) {
        while ( (writer + 1) % size == reader ); // full; need to wait
        q[ writer ] = x;
        writer = (writer + 1) % size;
    }

    T &front() {
        return q[ reader ];
    }

    void pop() {
        reader = (reader + 1) % size;
    }

    bool empty() {
        return reader == writer;
    }

    Ring() : reader( 0 ), writer( 0 ) {}
};

template< typename T >
struct Student {
    static const int width = 64;
    static const int size = 8;

    volatile int writer;
    T q[width*size];
    int reader;
    volatile int free_lines __attribute__((aligned(64)));

    void push(T x) {
        q[writer] = x;
        writer = (writer+1) % (size*width);
        if (writer%size == 0) {
            __sync_fetch_and_sub(&free_lines, 1);
            // NOTE: (free_lines < 0) can happen!
            while (free_lines<=0) ;
        }
    }

    T &front() {
        return q[reader];
    }

    void pop() {
        reader = (reader+1)%(width*size);
        if (reader%size == 0) {
            __sync_fetch_and_add(&free_lines, 1);
        }
    }

    bool empty() {
        // NOTE: (free_lines > width) can happen!
        return free_lines >= width && reader == writer;
    }

    Student() : writer(0), reader(0), free_lines(width) {}
};

template< typename T >
struct Linked {
    using element = T;
    struct Node {
        T value;
        Node *next;
    };

    Node * volatile reader;
    char _separation[ 128 ];
    Node * volatile writer;

    void push( T x ) {
        Node *n = new Node;
        n->value = x;
        writer->next = n; // n->next = (Node *) writer;
        writer = n;
    }

    T &front() {
        return reader->value;
    }

    void pop() {
        Node volatile *n = reader;
        ASSERT( reader->next );
        reader = reader->next;
        delete n;
    }

    bool empty() {
        return reader == writer;
    }

    Linked() {
        reader = writer = new Node();
        reader->next = 0;
    }
};

#ifdef BRICKS_HAVE_TBB

template< typename T >
struct LocklessQueue {
    tbb::concurrent_queue< T > q;
    using element = T;

    void push( T x ) {
        q.push( x );
    }

    T pop() {
        T res;
        q.try_pop( res ); /* does nothing to res on failure */
        return res;
    }

    bool empty() {
        return q.empty();
    }

    LocklessQueue() {}
};

#endif

template< typename Q >
struct Shared {
    using T = typename Q::element;
    std::shared_ptr< Q > q;
    void push( T t ) { q->push( t ); }
    T pop() { return q->pop(); }
    bool empty() { return q->empty(); }
    void flush() {}
    Shared() : q( new Q() ) {}
};

template< template< typename > class Q, typename T >
struct Chunked {
    using Chunk = std::deque< T >;
    using ChQ = Q< Chunk >;
    std::shared_ptr< ChQ > q;
    unsigned chunkSize;

    Chunk outgoing;
    Chunk incoming;

    void push( T t ) {
        outgoing.push_back( t );
        // std::cerr << "pushed " << outgoing.back() << std::endl;
        if ( outgoing.size() >= chunkSize )
            flush();
    }

    T pop() {
        // std::cerr << "pop: empty = " << incoming.empty() << std::endl;
        if ( incoming.empty() )
            incoming = q->pop();
        if ( incoming.empty() )
            return T();
        // std::cerr << "pop: found " << incoming.front() << std::endl;
        auto x = incoming.front();
        incoming.pop_front();
        return x;
    }

    void flush() {
        if ( !outgoing.empty() ) {
            // std::cerr << "flushing " << outgoing.size() << " items" << std::endl;
            Chunk tmp;
            std::swap( outgoing, tmp );
            q->push( std::move( tmp ) );

            /* A quickstart trick -- make first few chunks smaller. */
            if ( chunkSize < 64 )
                chunkSize = std::min( 2 * chunkSize, 64u );
        }
    }

    bool empty() {
        if ( incoming.empty() ) { /* try to get a fresh one */
            incoming = q->pop();
            // std::cerr << "pulled in " << incoming.size() << " items" << std::endl;
        }
        return incoming.empty();
    }

    Chunked() : q( new ChQ() ), chunkSize( 2 ) {}
};

template< typename Q >
struct InsertThread : Thread {
    Q *q;
    int items;
    std::mt19937 rand;
    std::uniform_int_distribution<> dist;

    InsertThread() {}

    void main() {
        ASSERT( q );
        for ( int i = 0; i < items; ++i )
            q->push( rand() );
    };
};

template< typename Q >
struct WorkThread : Thread {
    Q q;
    std::atomic< bool > *stop;
    int items;
    int id, threads;

    WorkThread() {}

    void main() {
        int step = items / 10;
        for ( int i = 1; i <= step; ++i )
            if ( id == i % threads )
                q.push( i );
        while ( !stop->load() ) {
            while ( !q.empty() ) {
                int i = q.pop();
                if ( !i )
                    continue;
                if ( i == items )
                    stop->store( true );
                if ( i + step <= items ) {
                    q.push( i + step );
                    q.push( i + step + items );
                }
            }
            q.flush();
        }
    }
};

template< int size >
struct padded {
    int i;
    char padding[ size - sizeof( int ) ];
    operator int() { return i; }
    padded( int i ) : i( i ) {}
    padded() : i( 0 ) {}
};

struct ShQueue : BenchmarkGroup
{
    ShQueue() {
        x.type = Axis::Quantitative;
        x.name = "threads";
        x.min = 1;
        x.max = 16;
        x.step = 1;

        y.type = Axis::Qualitative;
        y.name = "type";
        y.min = 0;
        y.step = 1;
#ifdef BRICKS_HAVE_TBB
        y.max = 3;
#else
        y.max = 1;
#endif
        y._render = []( int i ) {
            switch (i) {
                case 0: return "spinlock";
                case 1: return "lockless";
                case 2: return "chunked";
                case 3: return "hybrid";
                default: abort();
            }
        };
    }

    std::string describe() {
        return "category:shmem category:shqueue";
    }

    template< typename Q >
    void scale() {
        Q fifo;
        auto *t = new WorkThread< Q >[ p ];
        std::atomic< bool > stop( false );

        for ( int i = 0; i < p; ++i ) {
            t[ i ].q = fifo;
            t[ i ].items = 1000;
            t[ i ].id = i;
            t[ i ].threads = p;
            t[ i ].stop = &stop;
        }

        for ( int i = 0; i < p; ++i )
            t[ i ].start();

        for ( int i = 0; i < p; ++i )
            t[ i ].join();
    }

    template< typename T >
    void param() {
        switch (q) {
            case 0: return scale< Shared< LockedQueue< T > > >();
            case 1: return scale< Chunked< LockedQueue, T > >();
#ifdef BRICKS_HAVE_TBB
            case 2: return scale< Shared< LocklessQueue< T > > >();
            case 3: return scale< Chunked< LocklessQueue, T > >();
#endif
            default: ASSERT_UNREACHABLE_F( "bad q = %d", q );
        }
    }

    BENCHMARK(p_int) { param< int >(); }
    BENCHMARK(p_intptr) { param< intptr_t >(); }
    BENCHMARK(p_64b) { param< padded< 64 > >(); }
};

struct FIFO : BenchmarkGroup
{
    FIFO() {
        x.type = Axis::Disabled;
        /* x.name = "p";
        x.unit = "items";
        x.min = 8;
        x.max = 4096;
        x.log = true;
        x.step = 8; */

        y.type = Axis::Qualitative;
        y.name = "type";
        y.min = 0;
        y.step = 1;
        y.max = 4;
        y._render = []( int i ) {
            switch (i) {
                case 0: return "mutex";
                case 1: return "spin";
                case 2: return "linked";
                case 3: return "ring";
                case 4: return "hybrid";
                case 5: return "student";
                default: ASSERT_UNREACHABLE_F( "bad i = %d", i );
            }
        };
    }

    std::string describe() {
        return "category:shmem category:fifo";
    }

    template< typename Q >
    void length_() {
        Q fifo;
        InsertThread< Q > t;
        t.q = &fifo;
        t.items = 1024 * 1024;

        t.start();

        for ( int i = 0; i < t.items; ++i ) {
            while ( fifo.empty() );
            fifo.pop();
        }
        ASSERT( fifo.empty() );
    }

    template< typename T >
    void param() {
        switch (q) {
            case 0: return length_< Naive< T > >();
            case 1: return length_< LockedQueue< T > >();
            case 2: return length_< Linked< T > >();
            case 3: return length_< Ring< T  > >();
            case 4: return length_< Fifo< T > >();
            case 5: return length_< Student< T > >();
            default: ASSERT_UNREACHABLE_F( "bad q = %d", q );
        }
    }

    BENCHMARK(p_char) { param< char >(); }
    BENCHMARK(p_int) { param< int >(); }
    BENCHMARK(p_intptr) { param< intptr_t >(); }
    BENCHMARK(p_16b) { param< padded< 16 > >(); }
    BENCHMARK(p_64b) { param< padded< 64 > >(); }
};

}
}

#endif
#endif
#endif

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
