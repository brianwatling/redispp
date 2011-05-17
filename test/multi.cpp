#include <stdio.h>
#include <string>
#include <redispp.h>
#ifdef _WIN32
#include <Windows.h>
#endif

using namespace redispp;
using namespace std;

const char* TEST_PORT = "0";
const char* TEST_HOST = "127.0.0.1";

int main(int argc, char* argv[])
{
#ifdef _WIN32
    WSADATA wsaData;
    WORD version;
    version = MAKEWORD( 2, 0 );
    WSAStartup( version, &wsaData );
#endif

    Connection conn(TEST_HOST, TEST_PORT);
    int length = conn.llen("list");
    printf("Length: %d\n", length);
    if(argc > 1) {
        int targetLength = atoi(argv[1]);
        const size_t chunkFactor = 256;
        IntReply replies[chunkFactor];
        string key = "list";
        string value = "abcdefghijklmnopqrstuvwxyz";
        while(value.length() < 1400) {
            value = value + value;
        }
        for(int i = 0; length < targetLength; ++i, ++length) {
            replies[i & (chunkFactor - 1)] = conn.lpush(key, value);
        }
        length = conn.llen("list");
        printf("New Length: %d\n", length);
    }

    MultiBulkEnumerator result = conn.lrange("list", length-10000, -1);

    string data;
    while (result.next(&data)) {
        printf("Data: %s\n", data.c_str());
    }

    return 0;
}
