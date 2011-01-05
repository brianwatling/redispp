#include "redispp.h"
#include <sys/time.h>
#include <iostream>
#include <boost/bind.hpp>
#include <list>

using namespace redispp;

void runFunc(const char* port, size_t count)
{
    Connection conn("127.0.0.1", port, "password", false);

    std::string key = "somemediumkey2";
    std::string value = "somemediumvalue";

    const size_t chunkFactor = 256;
    VoidReply replies[chunkFactor];

    timeval begin;
    gettimeofday(&begin, NULL);

    for(size_t i = 0; i < count; ++i)
    {
        const size_t index = i & (chunkFactor - 1);
        replies[index] = conn.set(key, value);
        if(index == (chunkFactor - 1))
        {
            for(size_t j = 0; j < chunkFactor; ++j)
            {
                replies[j].result();
            }
        }
    }

    timeval end;
    gettimeofday(&end, NULL);

    const long long beginUsec = (long long)begin.tv_sec * 1000000LL + begin.tv_usec;
    const long long endUsec = (long long)end.tv_sec * 1000000LL + end.tv_usec;
    std::cout << count << " writes in " << (double)(endUsec - beginUsec)/1000000.0 << " ~= " << (double)count * 1000000.0/(double)(endUsec - beginUsec) << std::endl;

    for(size_t i = 0; i < chunkFactor; ++i)
    {
        replies[i].result();
    }

    if((std::string)conn.get("somemediumkey2") != "somemediumvalue")
    {
        throw std::runtime_error("somemediumkey2 != somemediumvalue");
    }
}

int main(int argc, char* argv[])
{
    if(argc <= 1)
    {
        std::cout << "usage: ./perf <port> [count]" << std::endl;
        return 1;
    }
    int count = argc > 2 ? atoi(argv[2]) : 100000;
    runFunc(argv[1], count);
    return 1;
}
