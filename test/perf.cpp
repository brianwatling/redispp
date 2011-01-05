#include "redispp.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>
#include <boost/bind.hpp>
#include <list>
#ifdef _WIN32
#include <Windows.h>
#endif
using namespace redispp;
using namespace boost::posix_time;

void runFunc(const char* port, size_t count)
{
    Connection conn("192.168.65.128", "6379", "", false);//"127.0.0.1", port, "password", false);

    std::string key = "somemediumkey2";
    std::string value = "somemediumvalue";

    const size_t chunkFactor = 256;
    VoidReply replies[chunkFactor];

    ptime begin(microsec_clock::local_time());

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

    ptime end(microsec_clock::local_time());

    //const long long beginUsec = (long long)begin.tv_sec * 1000000LL + begin.tv_usec;
    //const long long endUsec = (long long)end.tv_sec * 1000000LL + end.tv_usec;
    std::cout << count << " writes in " << (end - begin).total_microseconds() << " usecs ~= " << (double)count * 1000000.0/(double)(end - begin).total_microseconds() << std::endl;

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
#ifdef _WIN32
    WSADATA wsaData;
    WORD version;
    version = MAKEWORD( 2, 0 );
    WSAStartup( version, &wsaData );
#endif

    if(argc <= 1)
    {
        std::cout << "usage: ./perf <port> [count]" << std::endl;
        //return 1;
    }
    int count = argc > 2 ? atoi(argv[2]) : 100000;
    runFunc(argv[1], count);
    runFunc(argv[1], count);
    runFunc(argv[1], count);
    runFunc(argv[1], count);
    runFunc(argv[1], count);
    runFunc(argv[1], count);
    runFunc(argv[1], count);
    runFunc(argv[1], count);
    runFunc(argv[1], count);
    runFunc(argv[1], count);
    return 1;
}
