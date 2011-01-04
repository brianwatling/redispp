# Another C++ client for Redis

- Supports pipelining, using the same functions as synchronous requests
- The included performance test runs about 5 times faster with pipelining than with synchronous requests (on my laptop, to localhost)
- Depends on boost library
- g++, Linux, Mac (OS X 10.6.5)

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

## License

Public domain. Credit is appreciated and I would like to hear about how you use it.

## Author

- Brian Watling

