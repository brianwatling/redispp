# Another C++ client for Redis

- Supports pipelining, using the same functions as synchronous requests
- The included performance test runs about 5 times faster with pipelining than with synchronous requests (single client/thread, on my laptop, to localhost)
- Depends on boost library
- g++, tested on Linux, Mac (OS X 10.6.5)
- Written against Redis 2.0.4

## Simple example

    redispp::Connection conn("127.0.0.1", "6379", "password", false);
    conn.set("hello", "world");

## Pipelining Example

- Reply objects take care of reading the response lazily, on demand
- The response is read in either the destructor or when the return value is used
- Be sure reply objects are destroyed in the order they are created
- See test/perf.cpp or test/test.cpp for more examples

Up to 64 requests 'on the wire':
    VoidReply replies[64];

    for(size_t i = 0; i < count; ++i)
    {
        replies[i & 63] = conn.set(keys[i], values[i]);
    }

Save an object using pipelining. ~BoolReply takes care of reading the responses in order.
    BoolReply a = conn.hset("computer", "os", "linux");
    BoolReply b = conn.hset("computer", "speed", "3Ghz");
    BoolReply c = conn.hset("computer", "RAM", "8GB");
    BoolReply d = conn.hset("computer", "cores", "4");

Start loading a value, then use it later:
    StringReply value = conn.get("world");
    ...//do stuff
    std::string theValue = value;

These are resolved immediately:
    int hlen = conn.hlen("computer");
    std::string value = conn.get("world");

## Building

- You should be able to build libredispp.a and libredispp.so by typing 'make'
- Bjam users can type 'bjam'
- **WARNING** The unit tests will not pass unless you change TEST_PORT in test/test.cpp. The *entire* redis database will be cleared
- You should run the unit and performance tests with a temporary database, with no production data
- The performance test will not run unless you start it with a port (ie ./perf 6379)

## License

Public domain. Credit is appreciated and I would like to hear about how you use it.

## Author

- Brian Watling

