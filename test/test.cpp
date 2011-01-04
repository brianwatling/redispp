#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE RedisPPTest
#include <boost/test/included/unit_test.hpp>
#include <redispp.h>
#include <sys/time.h>

using namespace redispp;

BOOST_AUTO_TEST_CASE(set_get_exists_del)
{
    Connection conn("127.0.0.1", "6379", "password");

    conn.set("hello", "world");
std::cout << (std::string)conn.get("hello") << std::endl;
    BOOST_CHECK((std::string)conn.get("hello") == "world");
    BOOST_CHECK((bool)conn.exists("hello"));
    BOOST_CHECK((bool)conn.del("hello"));
    BOOST_CHECK(!conn.exists("hello"));
    BOOST_CHECK(!conn.del("hello"));
}

BOOST_AUTO_TEST_CASE(type)
{
    Connection conn("127.0.0.1", "6379", "password");
    conn.set("hello", "world");
    BOOST_CHECK(conn.type("hello") == String);
}

BOOST_AUTO_TEST_CASE(keys)
{
    Connection conn("127.0.0.1", "6379", "password");
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
    Connection conn("127.0.0.1", "6379", "password");
    conn.set("hello", "world");
    BOOST_CHECK((bool)conn.exists(conn.randomKey()));
}

BOOST_AUTO_TEST_CASE(rename_)
{
    Connection conn("127.0.0.1", "6379", "password");
    conn.set("hello", "world");
    conn.rename("hello", "hello1");
    BOOST_CHECK((std::string)conn.get("hello1") == "world");
    conn.set("hello2", "one");
    BOOST_CHECK(!conn.renameNX("hello1", "hello2"));
    BOOST_CHECK((bool)conn.renameNX("hello1", "hello3"));
}

BOOST_AUTO_TEST_CASE(dbsize)
{
    Connection conn("127.0.0.1", "6379", "password");
    conn.set("hello", "world");
    conn.del("hello1");
    int size = conn.dbSize();
    BOOST_CHECK(size >= 1);
    conn.set("hello1", "one");
    BOOST_CHECK(conn.dbSize() == size + 1);
}

BOOST_AUTO_TEST_CASE(expire_ttl)
{
    Connection conn("127.0.0.1", "6379", "password");
    conn.set("hello", "world");
    conn.set("hello1", "world");
    int now = time(NULL);
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
    Connection conn("127.0.0.1", "6379", "password");
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
    Connection conn("127.0.0.1", "6379", "password");
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
    Connection conn("127.0.0.1", "6379", "password");
    conn.set("hello", "world");
    BOOST_CHECK((std::string)conn.getSet("hello", "one") == "world");
    BOOST_CHECK((std::string)conn.get("hello") == "one");
}

BOOST_AUTO_TEST_CASE(setnx)
{
    Connection conn("127.0.0.1", "6379", "password");
    conn.set("hello", "world");
    BOOST_CHECK(!conn.setNX("hello", "one"));
    conn.del("hello");
    BOOST_CHECK((bool)conn.setNX("hello", "one"));
}

BOOST_AUTO_TEST_CASE(setex)
{
    Connection conn("127.0.0.1", "6379", "password");
    conn.setEx("hello", 5, "world");
    BOOST_CHECK(conn.ttl("hello") <= 5);
}

BOOST_AUTO_TEST_CASE(incrdecr)
{
    Connection conn("127.0.0.1", "6379", "password");
    conn.set("hello", "5");
    BOOST_CHECK(conn.incr("hello") == 6);
    BOOST_CHECK(conn.incrBy("hello", 2) == 8);
    BOOST_CHECK(conn.decr("hello") == 7);
    BOOST_CHECK(conn.decrBy("hello", 2) == 5);
}

BOOST_AUTO_TEST_CASE(append)
{
    Connection conn("127.0.0.1", "6379", "password");
    conn.set("hello", "world");
    BOOST_CHECK(conn.append("hello", "one") == 8);
    BOOST_CHECK((std::string)conn.get("hello") == "worldone");
}

BOOST_AUTO_TEST_CASE(substr)
{
    Connection conn("127.0.0.1", "6379", "password");
    conn.set("hello", "world");
    BOOST_CHECK((std::string)conn.subStr("hello", 1, 3) == "orl");
}

BOOST_AUTO_TEST_CASE(lists)
{
    Connection conn("127.0.0.1", "6379", "password");
    conn.del("hello");
    BOOST_CHECK(conn.lpush("hello", "c") == 1);
    BOOST_CHECK(conn.lpush("hello", "d") == 2);
    BOOST_CHECK(conn.rpush("hello", "b") == 3);
    BOOST_CHECK(conn.rpush("hello", "a") == 4);
    BOOST_CHECK(conn.llen("hello") == 4);
    MultiBulkEnumerator result = conn.lrange("hello", 1, 3);
    std::string str;
    BOOST_CHECK(result.next(&str));
    BOOST_CHECK(str == "c");
    BOOST_CHECK(result.next(&str));
    BOOST_CHECK(str == "b");
    BOOST_CHECK(result.next(&str));
    BOOST_CHECK(str == "a");
    conn.ltrim("hello", 0, 1);
    result = conn.lrange("hello", 0, 10);
    BOOST_CHECK(result.next(&str));
    BOOST_CHECK(str == "d");
    BOOST_CHECK(result.next(&str));
    BOOST_CHECK(str == "c");
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
}

BOOST_AUTO_TEST_CASE(sets)
{
    Connection conn("127.0.0.1", "6379", "password");
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
    Connection conn("127.0.0.1", "6379", "password");
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
}

BOOST_AUTO_TEST_CASE(misc)
{
    Connection conn("127.0.0.1", "6379", "password");
    int now = time(NULL);
    ::sleep(2);
    conn.save();
    BOOST_CHECK(conn.lastSave() > now);
    conn.bgSave();
    conn.bgReWriteAOF();
    BOOST_CHECK(((std::string)conn.info()).length() > 0);
}

//TODO: test for pipelined requests

