# Another C++ client for Redis

- Supports pipelining, using the same functions as synchronous requests
- The included performance test runs about 5 times faster with pipelining than with synchronous requests (single client/thread, on my laptop, to localhost)
- Depends on boost library
- Tested on Linux (g++), Windows (VC++ 2010), Mac (g++, OS X 10.6.5)
- Includes makefile, bjam jamfiles, and VC++ project
- Written against Redis 2.0.4

## Performance

This client's pipelining allows it to really push the redis server, even with one client. I ran the included performance benchmark on my Ubuntu 9.10 64-bit virtual machine. The physical machine is quad-core @ 3Ghz with 8GB RAM @ 800Mhz. At 256 requests 'on-the-wire', I reached approximately 230k writes per second with one connection (a write is: conn.set("somemediumkey2", "somemediumvalue")). I was able to reach nearly 260k writes per second with 4096 requests 'on-the-wire'. Below is test/perf.cpp's output for various amounts of outstanding requests.

256 requests on the wire:

    bwatling@ubuntu:~/Desktop/redispp$ for cur in `seq 1 10`; do ./test/bin/perf.test/gcc-4.4.1/release/perf 6379 1000000; done
    1000000 writes in 4451367 usecs ~= 224650 requests per second
    1000000 writes in 4301082 usecs ~= 232500 requests per second
    1000000 writes in 4294144 usecs ~= 232875 requests per second
    1000000 writes in 4255403 usecs ~= 234995 requests per second
    1000000 writes in 4272437 usecs ~= 234058 requests per second
    1000000 writes in 4273374 usecs ~= 234007 requests per second
    1000000 writes in 4251377 usecs ~= 235218 requests per second
    1000000 writes in 4288723 usecs ~= 233170 requests per second
    1000000 writes in 4247717 usecs ~= 235421 requests per second
    1000000 writes in 4257261 usecs ~= 234893 requests per second

4096 requests on the wire:

    bwatling@ubuntu:~/Desktop/redispp$ for cur in `seq 1 10`; do ./test/bin/perf.test/gcc-4.4.1/release/perf 6379 1000000; done
    1000000 writes in 4035970 usecs ~= 247772 requests per second
    1000000 writes in 3855737 usecs ~= 259354 requests per second
    1000000 writes in 3876598 usecs ~= 257958 requests per second
    1000000 writes in 3867489 usecs ~= 258566 requests per second
    1000000 writes in 3887749 usecs ~= 257218 requests per second
    1000000 writes in 3826811 usecs ~= 261314 requests per second
    1000000 writes in 3864827 usecs ~= 258744 requests per second
    1000000 writes in 3893552 usecs ~= 256835 requests per second
    1000000 writes in 3881562 usecs ~= 257628 requests per second
    1000000 writes in 3869083 usecs ~= 258459 requests per second

In comparison, with a single client and no pipelining this machine could handle approximately 50k writes per second (using redispp and the same for credis).

## Simple example

    redispp::Connection conn("127.0.0.1", "6379", "password", false);
    conn.set("hello", "world");

## Pipelining Example

- Reply objects take care of reading the response lazily, on demand
- The response is read in either the destructor or when the return value is used
- The objects can be nested/scoped in any order. All outstanding replies are read and cached for later when a newer request's response is used.
- See test/perf.cpp or test/test.cpp for more examples

Up to 64 requests 'on the wire':

    VoidReply replies[64];

    for(size_t i = 0; i < count; ++i)
    {
        replies[i & 63] = conn.set(keys[i], values[i]);
    }

Save an object using pipelining. ~BoolReply takes care of reading the responses in order.

    {
        BoolReply a = conn.hset("computer", "os", "linux");
        BoolReply b = conn.hset("computer", "speed", "3Ghz");
        BoolReply c = conn.hset("computer", "RAM", "8GB");
        BoolReply d = conn.hset("computer", "cores", "4");
    }
    //here all the replies have been cleared off conn's socket

Start loading a value, then use it later:

    StringReply value = conn.get("world");
    ...//do stuff
    std::string theValue = value;

These are resolved immediately:

    int hlen = conn.hlen("computer");
    std::string value = conn.get("world");

This demonstrates arbitrary nesting/scoping and works as expected (see test/test.cpp). There's no problems caused by a and readA outliving b and c:

    {
        VoidReply a = conn.set("one", "a");
        StringReply readA = conn.get("one");
        {
            BoolReply b = conn.hset("two", "two", "b");
            VoidReply c = conn.set("three", "c");
        }
        BOOST_CHECK(readA.result() == "a");
    }

## Multi Bulk Replies

Request that have multi-bulk replies supply a MultiBulkEnumerator as the return type. The MultiBulkEnumerator will read the data lazily as requested.

Read out a list:

    conn.lpush("hello", "a")
    conn.lpush("hello", "b")
    conn.lpush("hello", "c")
    MultiBulkEnumerator result = conn.lrange("hello", 1, 3);
    std::string result;
    while(result.next(&result))
        std::cout << result << std::endl;

## Transactions

The client has basic support for transactions. It currently can open a MULTI and close it with an EXEC. Closing with a DISCARD is not supported yet. WATCH and UNWATCH may also come soon. Here's an example of how to use transactions. Note: it's very important to use the defered reply objects with transactions, or else the connection will be corrupted. (see trans.cpp for more detail).

    Transaction trans(&conn);
    VoidReply one = conn.set("x", "1");
    VoidReply two = conn.set("y", "21");
    StringReply three = conn.get("x");
    trans.commit();
    //access one, two, and three here

## Building

- You should be able to build libredispp.a and libredispp.so by typing 'make'
- Bjam users can type 'bjam'
- Windows can use the included VC++ 2010 project file. Be warned I've set it up to simply call bjam. It should be fairly simple to create a regular project or include the source in your own.
- **WARNING** The unit tests will not pass unless you change TEST_PORT in test/test.cpp. The *entire* redis database will be cleared
- You should run the unit and performance tests with a temporary database, with no production data
- The performance test will not run unless you start it with a port (ie ./perf 6379)

## TODO

- add a way to listen for messages after subscribing to a channel
- fill in the missing requests
- cleanup code, move stuff out of the header to the .cpp file
- implement a clean method for watch and related functions (using transaction objects)
- write a consistent hashing wrapper?

## License

Public domain. Credit is appreciated and I would like to hear about how you use it.

## Author

- Brian Watling

