#include <stdio.h>
#include <string>
#include <redispp.h>
#ifdef _WIN32
#include <Windows.h>
#endif

using namespace redispp;
using namespace std;

const char* TEST_PORT = "6379";
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

    conn.set("x", "a");
    conn.set("y", "b");
    
    Transaction trans(&conn);
    VoidReply one = conn.set("x", "1");
    VoidReply two = conn.set("y", "21");
    StringReply resOne = conn.get("x");
    trans.commit();

    std::string res1 = resOne;
    std::string res2 = conn.get("x");
    if(res2 != res1)
        std::cout << "did not match: " << res1 << " " << res2 << std::endl;
    else
        std::cout << "they match: " << res1 << " " << res2 << std::endl;
    return 0;
}
