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

void runFunc(const char* arg, size_t count)
{
#ifdef UNIX_DOMAIN_SOCKET
    Connection conn(arg, NULL);
#else
    Connection conn("localhost", arg, NULL);
#endif

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

    std::cout << count << " writes in " << (end - begin).total_microseconds() << " usecs ~= " << (double)count * 1000000.0/(double)(end - begin).total_microseconds() << " requests per second" << std::endl;

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
#ifdef UNIX_DOMAIN_SOCKET
        std::cout << "usage: ./perf <socket> [count]" << std::endl;
#else
        std::cout << "usage: ./perf <port> [count]" << std::endl;
#endif
        return 1;
    }
    int count = argc > 2 ? atoi(argv[2]) : 100000;
    runFunc(argv[1], count);
    return 0;
}
