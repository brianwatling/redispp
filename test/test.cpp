#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>
#include <redispp.h>
#include <time.h>
#include <boost/assign/list_of.hpp>
#ifdef _WIN32
#include <windows.h>
void sleep(size_t seconds)
{
    Sleep(seconds * 1000);
}
#endif

using namespace redispp;

const char* TEST_PORT = "6379";
const char* TEST_HOST = "127.0.0.1";
const char* TEST_UNIX_DOMAIN_SOCKET = "/tmp/redis.sock";

bool init_unit_test()
{
#ifdef _WIN32
    WSADATA wsaData;
    WORD version;
    version = MAKEWORD( 2, 0 );
    WSAStartup( version, &wsaData );
#endif
	return true;
}

#ifdef UNIX_DOMAIN_SOCKET
struct F {
	F() : conn(TEST_UNIX_DOMAIN_SOCKET, "password") {BOOST_TEST_MESSAGE( "Set up fixture" );}
	~F() {BOOST_TEST_MESSAGE( "Tore down fixture" );}
	Connection conn;
};
#else
struct F {
	F() : conn(TEST_HOST, TEST_PORT, "password") {BOOST_TEST_MESSAGE( "Set up fixture" );}
	~F() {BOOST_TEST_MESSAGE( "Tore down fixture" );}
	Connection conn;
};
#endif

BOOST_FIXTURE_TEST_SUITE( s, F )

BOOST_AUTO_TEST_CASE(set_get_exists_del)
{
    conn.set("hello", "world");
    StringReply stringReply = conn.get("hello");
    BOOST_CHECK(stringReply.result().is_initialized());
    BOOST_CHECK_EQUAL((std::string)conn.get("hello"), "world");
    BOOST_CHECK((bool)conn.exists("hello"));
    BOOST_CHECK((bool)conn.del("hello"));
    BOOST_CHECK(!conn.exists("hello"));
    BOOST_CHECK(!conn.del("hello"));
}

BOOST_AUTO_TEST_CASE(nullreplies)
{
	BOOST_CHECK_THROW ((std::string)conn.get("nonexistant"), NullReplyException);
	StringReply nullReply = conn.get("nonexistant");
	BOOST_CHECK_EQUAL(false, nullReply.result().is_initialized());

	// Connection still in good state:
	conn.set("one", "1");
	BOOST_CHECK_EQUAL((std::string)conn.get("one"), "1");

    MultiBulkEnumerator result = conn.blpop(
            boost::assign::list_of("notalist"), 1);
    std::string str;
    BOOST_CHECK(!result.next(&str));

	// Connection still in good state:
	BOOST_CHECK_EQUAL((std::string)conn.get("one"), "1");

}

BOOST_AUTO_TEST_CASE(type)
{
    conn.set("hello", "world");
    BOOST_CHECK(conn.type("hello") == String);
}

BOOST_AUTO_TEST_CASE(keys)
{
    conn.set("hello", "world");
    MultiBulkEnumerator response = conn.keys("h?llo");
    std::string key;
    bool found = false;
    while(response.next(&key))
    {
        if(key == "hello")
            found = true;
    }
    BOOST_CHECK(found);
}

BOOST_AUTO_TEST_CASE(randomkey)
{
    conn.set("hello", "world");
    BOOST_CHECK((bool)conn.exists(conn.randomKey()));
}

BOOST_AUTO_TEST_CASE(rename_)
{
    conn.set("hello", "world");
    conn.rename("hello", "hello1");
    BOOST_CHECK((std::string)conn.get("hello1") == "world");
    conn.set("hello2", "one");
    BOOST_CHECK(!conn.renameNX("hello1", "hello2"));
    BOOST_CHECK((bool)conn.renameNX("hello1", "hello3"));
}

BOOST_AUTO_TEST_CASE(dbsize)
{
    conn.set("hello", "world");
    conn.del("hello1");
    int size = conn.dbSize();
    BOOST_CHECK(size >= 1);
    conn.set("hello1", "one");
    BOOST_CHECK(conn.dbSize() == size + 1);
}

BOOST_AUTO_TEST_CASE(expire_ttl)
{
    conn.set("hello", "world");
    conn.set("hello1", "world");
    time_t now = time(NULL);
    BOOST_CHECK((bool)conn.expire("hello", 5));
    BOOST_CHECK((bool)conn.expireAt("hello1", now + 5));
    BOOST_CHECK(conn.ttl("hello") <= 5);
    BOOST_CHECK(conn.ttl("hello1") <= 5);
    sleep(6);
    BOOST_CHECK(!conn.exists("hello"));
    BOOST_CHECK(!conn.exists("hello1"));
}

BOOST_AUTO_TEST_CASE(select_move)
{
    conn.select(0);
    conn.set("hello", "world");
    BOOST_CHECK((bool)conn.exists("hello"));
    conn.select(1);
    BOOST_CHECK(!conn.exists("hello"));
    conn.select(0);
    conn.move("hello", 1);
    BOOST_CHECK(!conn.exists("hello"));
    conn.select(1);
    BOOST_CHECK((bool)conn.exists("hello"));
    conn.select(0);
}

BOOST_AUTO_TEST_CASE(flush)
{
    conn.set("hello", "world");
    BOOST_CHECK(conn.dbSize() > 0);
    conn.flushDb();
    BOOST_CHECK(conn.dbSize() == 0);
    conn.set("hello", "world");
    BOOST_CHECK(conn.dbSize() > 0);
    conn.select(1);
    conn.set("hello", "world");
    BOOST_CHECK(conn.dbSize() > 0);
    conn.flushAll();
    BOOST_CHECK(conn.dbSize() == 0);
    conn.select(0);
    BOOST_CHECK(conn.dbSize() == 0);
}

BOOST_AUTO_TEST_CASE(getset)
{
    conn.set("hello", "world");
    BOOST_CHECK((std::string)conn.getSet("hello", "one") == "world");
    BOOST_CHECK((std::string)conn.get("hello") == "one");
}

BOOST_AUTO_TEST_CASE(setnx)
{
    conn.set("hello", "world");
    BOOST_CHECK(!conn.setNX("hello", "one"));
    conn.del("hello");
    BOOST_CHECK((bool)conn.setNX("hello", "one"));
}

BOOST_AUTO_TEST_CASE(setex)
{
    conn.setEx("hello", 5, "world");
    BOOST_CHECK(conn.ttl("hello") <= 5);
}

BOOST_AUTO_TEST_CASE(incrdecr)
{
    conn.set("hello", "5");
    BOOST_CHECK(conn.incr("hello") == 6);
    BOOST_CHECK(conn.incrBy("hello", 2) == 8);
    BOOST_CHECK(conn.decr("hello") == 7);
    BOOST_CHECK(conn.decrBy("hello", 2) == 5);
}

BOOST_AUTO_TEST_CASE(append)
{
    conn.set("hello", "world");
    BOOST_CHECK(conn.append("hello", "one") == 8);
    BOOST_CHECK((std::string)conn.get("hello") == "worldone");
}

BOOST_AUTO_TEST_CASE(substr)
{
    conn.set("hello", "world");
    BOOST_CHECK((std::string)conn.subStr("hello", 1, 3) == "orl");
}

BOOST_AUTO_TEST_CASE(lists)
{
    conn.del("hello");
    BOOST_CHECK(conn.lpush("hello", "c") == 1);
    BOOST_CHECK(conn.lpush("hello", "d") == 2);
    BOOST_CHECK(conn.rpush("hello", "b") == 3);
    BOOST_CHECK(conn.rpush("hello", "a") == 4);
    BOOST_CHECK(conn.llen("hello") == 4);
    MultiBulkEnumerator result = conn.lrange("hello", 1, 3);
    std::string str1;
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(str1 == "c");
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(str1 == "b");
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(str1 == "a");
    conn.ltrim("hello", 0, 1);
    result = conn.lrange("hello", 0, 10);
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(str1 == "d");
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(str1 == "c");
    BOOST_CHECK((std::string)conn.lindex("hello", 0) == "d");
    BOOST_CHECK((std::string)conn.lindex("hello", 1) == "c");
    conn.lset("hello", 1, "f");
    BOOST_CHECK((std::string)conn.lindex("hello", 1) == "f");
    conn.lpush("hello", "f");
    conn.lpush("hello", "f");
    conn.lpush("hello", "f");
    BOOST_CHECK(conn.lrem("hello", 2, "f") == 2);
    BOOST_CHECK(conn.llen("hello") == 3);
    BOOST_CHECK((std::string)conn.lpop("hello") == "f");
    BOOST_CHECK(conn.llen("hello") == 2);
    conn.rpush("hello", "x");
    BOOST_CHECK((std::string)conn.rpop("hello") == "x");
    conn.rpush("hello", "z");
    BOOST_CHECK((std::string)conn.rpopLpush("hello", "hello") == "z");
    conn.lpush("list1", "a");
    conn.lpush("list1", "b");
    conn.lpush("list2", "c");
    result = conn.blpop(boost::assign::list_of("list1")("list2"), 0);
    std::string str2;
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(result.next(&str2));
    BOOST_CHECK(str1 == "list1");
    BOOST_CHECK(str2 == "b");
    result = conn.blpop(boost::assign::list_of("list1")("list2"), 0);
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(result.next(&str2));
    BOOST_CHECK(str1 == "list1");
    BOOST_CHECK(str2 == "a");
    result = conn.blpop(boost::assign::list_of("list1")("list2"), 0);
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(result.next(&str2));
    BOOST_CHECK(str1 == "list2");
    BOOST_CHECK(str2 == "c");
    result = conn.blpop(boost::assign::list_of("list1")("list2"), 1);
	BOOST_CHECK(!result.next(&str1));
    conn.lpush("list1", "a");
    conn.lpush("list1", "b");
    conn.lpush("list2", "c");
    result = conn.brpop(boost::assign::list_of("list1")("list2"), 0);
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(result.next(&str2));
    BOOST_CHECK(str1 == "list1");
    BOOST_CHECK(str2 == "a");
    result = conn.brpop(boost::assign::list_of("list1")("list2"), 0);
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(result.next(&str2));
    BOOST_CHECK(str1 == "list1");
    BOOST_CHECK(str2 == "b");
    result = conn.brpop(boost::assign::list_of("list1")("list2"), 0);
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(result.next(&str2));
    BOOST_CHECK(str1 == "list2");
    BOOST_CHECK(str2 == "c");
    result = conn.brpop(boost::assign::list_of("list1")("list2"), 1);
	BOOST_CHECK(!result.next(&str1));
}

BOOST_AUTO_TEST_CASE(sets)
{
    conn.del("hello");
    BOOST_CHECK((bool)conn.sadd("hello", "world"));
    BOOST_CHECK((bool)conn.sisMember("hello", "world"));
    BOOST_CHECK(!conn.sisMember("hello", "mars"));
    BOOST_CHECK(conn.scard("hello") == 1);
    BOOST_CHECK((bool)conn.sadd("hello", "mars"));
    BOOST_CHECK(conn.scard("hello") == 2);
    MultiBulkEnumerator result = conn.smembers("hello");
    std::string str1;
    std::string str2;
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(result.next(&str2));
    BOOST_CHECK((str1 == "world" && str2 == "mars") || (str2 == "world" && str1 == "mars"));
    std::string randomMember = conn.srandMember("hello");
    BOOST_CHECK(randomMember == "world" || randomMember == "mars");
    BOOST_CHECK((bool)conn.srem("hello", "mars"));
    BOOST_CHECK(conn.scard("hello") == 1);
    BOOST_CHECK((std::string)conn.spop("hello") == "world");
    BOOST_CHECK(conn.scard("hello") == 0);
    conn.del("hello1");
    BOOST_CHECK((bool)conn.sadd("hello", "world"));
    BOOST_CHECK(conn.scard("hello") == 1);
    BOOST_CHECK((bool)conn.smove("hello", "hello1", "world"));
    BOOST_CHECK(conn.scard("hello") == 0);
    BOOST_CHECK(conn.scard("hello1") == 1);
}

BOOST_AUTO_TEST_CASE(hashes)
{
    conn.del("hello");
    BOOST_CHECK((bool)conn.hset("hello", "world", "one"));
    BOOST_CHECK((bool)conn.hset("hello", "mars", "two"));
    BOOST_CHECK((std::string)conn.hget("hello", "world") == "one");
    BOOST_CHECK(!conn.hsetNX("hello", "mars", "two"));
    BOOST_CHECK((bool)conn.hsetNX("hello", "venus", "1"));
    BOOST_CHECK(conn.hincrBy("hello", "venus", 3) == 4);
    BOOST_CHECK((bool)conn.hexists("hello", "venus"));
    BOOST_CHECK((bool)conn.hdel("hello", "venus"));
    BOOST_CHECK(!conn.hexists("hello", "venus"));
    BOOST_CHECK(conn.hlen("hello") == 2);
    MultiBulkEnumerator result = conn.hkeys("hello");
    std::string str1;
    std::string str2;
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(result.next(&str2));
    BOOST_CHECK((str1 == "world" && str2 == "mars") || (str2 == "world" && str1 == "mars"));
    result = conn.hvals("hello");
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(result.next(&str2));
    BOOST_CHECK((str1 == "one" && str2 == "two") || (str2 == "one" && str1 == "two"));
    result = conn.hgetAll("hello");
    std::string str3;
    std::string str4;
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(result.next(&str2));
    BOOST_CHECK(result.next(&str3));
    BOOST_CHECK(result.next(&str4));
    BOOST_CHECK(
                    (str1 == "world" && str2 == "one" && str3 == "mars" && str4 == "two")
                    ||
                    (str1 == "mars" && str2 == "two" && str3 == "world" && str4 == "one")
                );
    KeyValueList fields = boost::assign::list_of
        (std::make_pair("venus", "three"))
        (std::make_pair("jupiter", "four"));
    BOOST_CHECK((bool)conn.hmset("hello", fields));
    ArgList fieldNames = boost::assign::list_of("venus")("jupiter");
    result = conn.hmget("hello", fieldNames);
    BOOST_CHECK(result.next(&str1));
    BOOST_CHECK(result.next(&str2));
    BOOST_CHECK(str1 == "three" && str2 == "four");
}

BOOST_AUTO_TEST_CASE(misc)
{
    time_t now = time(NULL);
    ::sleep(2);
    conn.save();
    BOOST_CHECK(conn.lastSave() > now);
    conn.bgSave();
    conn.bgReWriteAOF();
    BOOST_CHECK(((std::string)conn.info()).length() > 0);
}

//TODO: test for pipelined requests

BOOST_AUTO_TEST_CASE(pipelined)
{
    {
        VoidReply a = conn.set("one", "a");
        StringReply readA = conn.get("one");
        {
            BoolReply b = conn.hset("two", "two", "b");
            VoidReply c = conn.set("three", "c");
        }
        BOOST_CHECK((std::string)readA == "a");
    }

    BOOST_CHECK((std::string)conn.get("one") == "a");
    BOOST_CHECK((std::string)conn.hget("two", "two") == "b");
    BOOST_CHECK((std::string)conn.get("three") == "c");

    {
        conn.del("hello");
        IntReply c = conn.lpush("hello", "c");
        IntReply d = conn.lpush("hello", "d");
        IntReply b = conn.rpush("hello", "b");
        IntReply a = conn.rpush("hello", "a");
        {
            MultiBulkEnumerator result = conn.lrange("hello", 1, 3);
            IntReply c = conn.lpush("hello", "c");
            IntReply d = conn.lpush("hello", "d");
            IntReply b = conn.rpush("hello", "b");
            IntReply a = conn.rpush("hello", "a");
            BOOST_CHECK((int)a == 8);
            BOOST_CHECK((int)b == 7);
            BOOST_CHECK((int)d == 6);
            BOOST_CHECK((int)c == 5);
            std::string str;
            BOOST_CHECK(result.next(&str));
            BOOST_CHECK(str == "c");
            BOOST_CHECK(result.next(&str));
            BOOST_CHECK(str == "b");
            BOOST_CHECK(result.next(&str));
            BOOST_CHECK(str == "a");
        }
        IntReply len = conn.llen("hello");
        BOOST_CHECK((int)a == 4);
        BOOST_CHECK((int)b == 3);
        BOOST_CHECK((int)d == 2);
        BOOST_CHECK((int)c == 1);
    }
}

BOOST_AUTO_TEST_SUITE_END()

