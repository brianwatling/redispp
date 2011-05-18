#include "redispp.h"
#include <errno.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

typedef int ssize_t;
typedef char* RecvBufferType;

int close(SOCKET sock)
{
    return closesocket(sock);
}

static bool setSocketFlag(SOCKET sock, int level, int optname, bool value)
{
    BOOL val = value ? TRUE : FALSE;
    return 0 == setsockopt(sock, level, optname, (char*)&val, sizeof(value));
}

static const char* getLastErrorMessage()
{
    return gai_strerror(WSAGetLastError());
}

#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netdb.h>

typedef int SOCKET;
typedef void* RecvBufferType;

static bool setSocketFlag(SOCKET sock, int level, int optname, bool value)
{
    int val = value ? 1 : 0;
    return 0 == setsockopt(sock, level, optname, &val, sizeof(value));
}

static const char* getLastErrorMessage()
{
    return strerror(errno);
}

#endif
#include <stdio.h>
#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/lexical_cast.hpp>
#include <assert.h>

namespace redispp
{

class ClientSocket : boost::noncopyable
{
public:
    ClientSocket(const char* host, const char* port)
    : sockFd(-1), streamBuf(this)
    {
        struct addrinfo hints;
        struct addrinfo* res = NULL;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;// use IPv4 or IPv6, whichever
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;// fill in my IP for me
        if(getaddrinfo(host, port, &hints, &res))
        {
            throw std::runtime_error(std::string("error getting address info for ") + host + ":" + port + " (" + getLastErrorMessage() + ")");
        }

        sockFd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if(sockFd < 0)
        {
            freeaddrinfo(res);
            throw std::runtime_error(std::string("error connecting to ") + host + ":" + port + " (" + getLastErrorMessage() + ")");
        }

        setSocketFlag(sockFd, SOL_SOCKET, SO_REUSEADDR, true);
        setSocketFlag(sockFd, SOL_SOCKET, SO_KEEPALIVE, true);

        if(connect(sockFd, res->ai_addr, res->ai_addrlen))
        {
            freeaddrinfo(res);
            close(sockFd);
            throw std::runtime_error(std::string("error connecting to ") + host + ":" + port + "(" + getLastErrorMessage() + ")");
        }

        freeaddrinfo(res);
    }

    void tcpNoDelay(bool enable)
    {
        const bool ret = setSocketFlag(sockFd, IPPROTO_TCP, TCP_NODELAY, enable);
        if(!ret)
        {
            throw std::runtime_error(std::string("error setting TCP_NODELAY: ") + getLastErrorMessage());
        }
    }

    void write(const void* data, size_t len)
    {
        size_t sent = 0;
        while(sent < len)
        {
            const ssize_t ret = ::send(sockFd, (const char*)data + sent, len - sent, 0);
            if(ret <= 0)
            {
                throw std::runtime_error(std::string("error writing to socket: ") + getLastErrorMessage());
            }
            sent += ret;
        }
    }

    size_t read(void* data, size_t len)
    {
        const ssize_t ret = ::recv(sockFd, (RecvBufferType)data, len, 0);
        if(ret <= 0)
        {
            throw std::runtime_error(std::string("error reading from socket: ") + getLastErrorMessage());
        }
        return ret;
    }

    ~ClientSocket()
    {
        if(sockFd >= 0)
        {
            close(sockFd);
        }
    }

    std::streambuf* getStreamBuf()
    {
        return &streamBuf;
    }

    class StreamBuf : public std::streambuf
    {
    public:
        StreamBuf(ClientSocket* conn)
        : conn(conn)
        {
            setp(outBuffer, outBuffer + sizeof(outBuffer) - 1);
        }

        int_type overflow(int_type c)
        {
            if (!traits_type::eq_int_type(traits_type::eof(), c))
            {
                traits_type::assign(*pptr(), traits_type::to_char_type(c));
                pbump(1);
            }
            return sync() == 0 ? traits_type::not_eof(c): traits_type::eof();
        }

        std::streamsize xsputn(const char* buf, std::streamsize size)
        {
            sync();
            conn->write(buf, size);
            return size;
        }

        int sync()
        {
            if(pbase() != pptr())
            {
                conn->write(pbase(), pptr() - pbase());
                setp(outBuffer, outBuffer + sizeof(outBuffer) - 1);
            }
            return 1;
        }

        int_type underflow()
        {
            const size_t got = conn->read(inBuffer, sizeof(inBuffer));
            setg(inBuffer, inBuffer, inBuffer + got);
            return traits_type::to_int_type(*gptr());
        }

        std::streamsize xsgetn(char* dest, std::streamsize size)
        {
            std::streamsize numRead = 0;
            char* const beg = gptr();
            char* const end = egptr();
            if(beg < end)
            {
                const std::streamsize avail = end - beg;
                numRead = std::min<std::streamsize>(size, avail);
                char* const newBeg = beg + numRead;
                std::copy(beg, newBeg, dest);
                if(newBeg != end)
                {
                    setg(newBeg, newBeg, end);
                    return numRead;
                }
            }
            while(numRead < size)
            {
                numRead += conn->read(dest + numRead, size - numRead);
            }
            setg(inBuffer, inBuffer, inBuffer);
            return numRead;
        }

    private:
        char outBuffer[1400];
        char inBuffer[1400];
        ClientSocket* conn;
    };

private:
    SOCKET sockFd;
    StreamBuf streamBuf;
};

#ifndef REDISPP_ALTBUFFER
class Buffer
{
public:
    Buffer(size_t bufferSize)
    : buffer(new char[bufferSize]), spot(buffer), end(buffer + bufferSize), marked(buffer)
    {}

    void write(char c)
    {
        checkSpace(sizeof(char));
        *spot = c;
        ++spot;
    }

    void write(const char* str)
    {
        const size_t len = strlen(str);
        checkSpace(len);
        memcpy(spot, str, len);
        spot += len;
    }

    void write(const char* str, size_t len)
    {
        checkSpace(len);
        memcpy(spot, str, len);
        spot += len;
    }

    void write(const std::string& str)
    {
        write(str.c_str(), str.size());
    }

    void write(size_t i)
    {
        namespace qi = boost::spirit::qi;
        namespace karma = boost::spirit::karma;
        namespace ascii = boost::spirit::ascii;
        using karma::uint_;
        using karma::generate;
        checkSpace(11);
        generate(spot, uint_, i);
    }

    template<typename T>
    void writeArg(T const& a);

    void mark()
    {
        marked = spot;
    }

    void reset()
    {
        spot = buffer;
        marked = buffer;
    }

    void resetToMark()
    {
        spot = marked;
    }

    void checkSpace(size_t needed)
    {
        if(spot + needed >= end)
        {
            throw std::runtime_error("buffer is full: spot + needed >= end");
        }
    }

    size_t length() const
    {
        return spot - buffer;
    }

    char* data()
    {
        return buffer;
    }

    const char* data() const
    {
        return buffer;
    }

private:
    void writeArgLen(unsigned int len)
    {
        namespace qi = boost::spirit::qi;
        namespace karma = boost::spirit::karma;
        namespace ascii = boost::spirit::ascii;
        using karma::uint_;
        using karma::generate;

        *spot++ = '$';
        generate(spot, uint_, len);
        *spot++ = '\r';
        *spot++ = '\n';
    }

    char* buffer;
    char* spot;
    char* end;
    char* marked;
};

template<>
void Buffer::writeArg<const char*>(const char* const& arg)
{
    const size_t len = strlen(arg);
    checkSpace(len + 1 + 11 + 4);//1 $, 11 for length, 4 for \r\n's
    writeArgLen(len);
    memcpy(spot, arg, len);
    spot += len;
    *spot++ = '\r';
    *spot++ = '\n';
}

template<>
void Buffer::writeArg<std::string>(std::string const& arg)
{
    const size_t len = arg.length();
    checkSpace(len + 1 + 11 + 4);//1 $, 11 for length, 4 for \r\n's
    writeArgLen(len);
    memcpy(spot, &(arg[0]), len);
    spot += len;
    *spot++ = '\r';
    *spot++ = '\n';
}

template<>
void Buffer::writeArg<int>(int const& arg)
{
    namespace qi = boost::spirit::qi;
    namespace karma = boost::spirit::karma;
    namespace ascii = boost::spirit::ascii;
    using karma::int_;
    using karma::generate;

    char numberBuf[12];
    char* spot = numberBuf;
    generate(spot, int_, arg);
    *spot = 0;
    writeArg((const char*)numberBuf);
}

#define EXECUTE_COMMAND_SYNC(cmd) \
    do {\
        buffer->resetToMark(); \
        _ ## cmd ## Command.execute(buffer); \
        ioStream->write(buffer->data(), buffer->length()); \
    } while(0)

#define EXECUTE_COMMAND_SYNC1(cmd, arg1) \
    do {\
        buffer->resetToMark(); \
        _ ## cmd ## Command.execute(arg1, buffer); \
        ioStream->write(buffer->data(), buffer->length()); \
    } while(0)

#define EXECUTE_COMMAND_SYNC2(cmd, arg1, arg2) \
    do {\
        buffer->resetToMark(); \
        _ ## cmd ## Command.execute(arg1, arg2, buffer); \
        ioStream->write(buffer->data(), buffer->length()); \
    } while(0)

#define EXECUTE_COMMAND_SYNC3(cmd, arg1, arg2, arg3) \
    do {\
        buffer->resetToMark(); \
        _ ## cmd ## Command.execute(arg1, arg2, arg3, buffer); \
        ioStream->write(buffer->data(), buffer->length()); \
    } while(0)

#else

class Buffer
{
public:
    Buffer(std::iostream& out)
    : out(out)
    {}

    void write(char c)
    {
        out << c;
    }

    void write(const char* str)
    {
        out << str;
    }

    void write(const char* str, size_t len)
    {
        out << str;
    }

    void write(const std::string& str)
    {
        out << str;
    }

    void write(size_t i)
    {
        out << i;
    }

    template<typename T>
    void writeArg(T const& a);

private:
    void writeArgLen(unsigned int len)
    {
        out << '$' << len << "\r\n";
    }

    std::iostream& out;
};

template<>
void Buffer::writeArg<const char*>(const char* const& arg)
{
    const size_t len = strlen(arg);
    writeArgLen(len);
    out << arg << "\r\n";
}

template<>
void Buffer::writeArg<std::string>(std::string const& arg)
{
    writeArgLen(arg.length());
    out << arg << "\r\n";
}

template<>
void Buffer::writeArg<int>(int const& arg)
{
    namespace qi = boost::spirit::qi;
    namespace karma = boost::spirit::karma;
    namespace ascii = boost::spirit::ascii;
    using karma::int_;
    using karma::generate;

    char numberBuf[12];
    char* spot = numberBuf;
    generate(spot, int_, arg);
    *spot = 0;
    writeArg((const char*)numberBuf);
}

#define EXECUTE_COMMAND_SYNC(cmd) \
    do {\
        _ ## cmd ## Command.execute(buffer); \
    } while(0)

#define EXECUTE_COMMAND_SYNC1(cmd, arg1) \
    do {\
        _ ## cmd ## Command.execute(arg1, buffer); \
    } while(0)

#define EXECUTE_COMMAND_SYNC2(cmd, arg1, arg2) \
    do {\
        _ ## cmd ## Command.execute(arg1, arg2, buffer); \
    } while(0)

#define EXECUTE_COMMAND_SYNC3(cmd, arg1, arg2, arg3) \
    do {\
        _ ## cmd ## Command.execute(arg1, arg2, arg3, buffer); \
    } while(0)

#endif

Command::Command(const char* cmdName, size_t numArgs)
{
    header = "*";
    header += boost::lexical_cast<std::string>(numArgs + 1);
    header += "\r\n";
    header += "$";
    header += boost::lexical_cast<std::string>(strlen(cmdName));
    header += "\r\n";
    header += cmdName;
    header += "\r\n";
}

Command::~Command()
{
}

BaseReply::BaseReply(Connection* conn)
: conn(conn)
{
    conn->outstandingReplies.push_back(*this);
    if(conn->transaction)
    {
        conn->transaction->replies.count += 1;
    }
}

BaseReply::BaseReply(const BaseReply& other)
: conn(other.conn)
{
    other.conn = NULL;
    if(conn)
        conn->outstandingReplies.insert(conn->outstandingReplies.iterator_to(other), *this);
    const_cast<BaseReply&>(other).unlink();
}

BaseReply& BaseReply::operator=(const BaseReply& other)
{
    unlink();
    conn = other.conn;
    if(conn)
        conn->outstandingReplies.insert(conn->outstandingReplies.iterator_to(other), *this);
    other.conn = NULL;
    const_cast<BaseReply&>(other).unlink();
    return *this;
}

void BaseReply::clearPendingResults()
{
    ReplyList::iterator cur = conn->outstandingReplies.begin();
    ReplyList::iterator const end = conn->outstandingReplies.iterator_to(*this);
    while(cur != end)
    {
        BaseReply& reply = *cur;
        ++cur;
        reply.readResult();
    }
}

VoidReply::VoidReply(Connection* conn)
: BaseReply(conn), storedResult(false)
{}

VoidReply::~VoidReply()
{
    try
    {
        result();
    }
    catch(...)
    {}
}

bool VoidReply::result()
{
    if(conn)
    {
#ifdef REDISPP_ALTBUFFER
        conn->ioStream->flush();
#endif
        clearPendingResults();
        Connection* const tmp = conn;
        conn = NULL;
        tmp->readStatusCodeReply();
        storedResult = true;
        unlink();
    }
    return storedResult;
}

BoolReply::BoolReply(Connection* conn)
: BaseReply(conn), storedResult(false)
{}

BoolReply::~BoolReply()
{
    try
    {
        result();
    }
    catch(...)
    {}
}

bool BoolReply::result()
{
    if(conn)
    {
#ifdef REDISPP_ALTBUFFER
        conn->ioStream->flush();
#endif
        clearPendingResults();
        Connection* const tmp = conn;
        conn = NULL;
        storedResult = tmp->readIntegerReply() > 0;
        unlink();
    }
    return storedResult;
}

IntReply::IntReply(Connection* conn)
: BaseReply(conn), storedResult(0)
{}

IntReply::~IntReply()
{
    try
    {
        result();
    }
    catch(...)
    {}
}

int IntReply::result()
{
    if(conn)
    {
#ifdef REDISPP_ALTBUFFER
        conn->ioStream->flush();
#endif
        clearPendingResults();
        Connection* const tmp = conn;
        conn = NULL;
        storedResult = tmp->readIntegerReply();
        unlink();
    }
    return storedResult;
}

StringReply::StringReply(Connection* conn)
: BaseReply(conn)
{}

StringReply::~StringReply()
{
    try
    {
        result();
    }
    catch(...)
    {}
}

const std::string& StringReply::result()
{
    if(conn)
    {
#ifdef REDISPP_ALTBUFFER
        conn->ioStream->flush();
#endif
        clearPendingResults();
        Connection* const tmp = conn;
        conn = NULL;
        tmp->readBulkReply(&storedResult);
        unlink();
    }
    return storedResult;
}

MultiBulkEnumerator::MultiBulkEnumerator(Connection* conn)
: BaseReply(conn), headerDone(false), count(0)
{}

MultiBulkEnumerator::~MultiBulkEnumerator()
{
    try
    {
        if(conn && count > 0)
        {
            std::string tmp;
            while(next(&tmp));
        }
    }
    catch(...)
    {}
}

bool MultiBulkEnumerator::next(std::string* out)
{
    if(!pending.empty())
    {
        *out = pending.front();
        pending.pop_front();
        return true;
    }
    if(!conn)
    {
        return false;
    }
    if(!headerDone)
    {
#ifdef REDISPP_ALTBUFFER
        conn->ioStream->flush();
#endif
        clearPendingResults();
        headerDone = true;
        char code = 0;
        if(!(*conn->ioStream >> code >> count))
        {
            throw std::runtime_error("error reading bulk response header");
        }
        if(code != '*')
        {
            throw std::runtime_error(std::string("bad bulk header code: ") + code);
        }
        if(count < 0)
        {
            throw std::runtime_error("multi bulk reply: -1");
        }
    }
    if(count <= 0)
    {
        conn = NULL;
        unlink();
        return false;
    }
    --count;
    *out = conn->readBulkReply();
    return true;
}

Connection::Connection(const char* host, const char* port, const char* password, bool noDelay)
: connection(new ClientSocket(host, port)), ioStream(new std::iostream(connection->getStreamBuf())),
#ifndef REDISPP_ALTBUFFER
  buffer(new Buffer(100*1024))
#else
  buffer(new Buffer(*ioStream))
#endif
  , transaction(NULL)
{
    if(noDelay)
    {
        connection->tcpNoDelay(true);
    }

    if(password)
    {
        authenticate(password);
    }
}

Connection::~Connection()
{
    ioStream.reset();//make sure this is cleared first, since it references the connection
}

static inline std::istream& getlineRN(std::istream& is, std::string& str)
{
    return std::getline(is, str, '\r');
}

std::string Connection::readStatusCodeReply()
{
    std::string ret;
    readStatusCodeReply(&ret);
    return ret;
}

void Connection::readStatusCodeReply(std::string* out)
{
    char code = 0;
    if(!(*ioStream >> code) || !getlineRN(*ioStream, *out))
    {
        throw std::runtime_error("error reading status response");
    }
    if(code != '+')
    {
        throw std::runtime_error(std::string("read error response: ") + *out);
    }
}

int Connection::readIntegerReply()
{
    char code = 0;
    int ret = 0;
    if(!(*ioStream >> code >> ret))
    {
        throw std::runtime_error("error reading integer response");
    }
    return ret;
}

std::string Connection::readBulkReply()
{
    std::string ret;
    readBulkReply(&ret);
    return ret;
}

void Connection::readBulkReply(std::string* out)
{
    char code = 0;
    int count = 0;
    if(!(*ioStream >> code >> count))
    {
        throw std::runtime_error("error reading bulk response header");
    }
    if(code != '$')
    {
        throw std::runtime_error(std::string("bad bulk header code: ") + code);
    }
    if(count < 0)
    {
        throw std::runtime_error("bulk reply: -1");
    }
    ioStream->get();//'\r'
    ioStream->get();//'\n'
    out->resize(count, '\0');
    ioStream->read((char*)out->c_str(), out->size());
}

void Connection::quit()
{
    EXECUTE_COMMAND_SYNC(Quit);
}

VoidReply Connection::authenticate(const char* password)
{
    EXECUTE_COMMAND_SYNC1(Auth, password);
    return VoidReply(this);
}

BoolReply Connection::exists(const std::string& name)
{
    EXECUTE_COMMAND_SYNC1(Exists, name);
    return BoolReply(this);
}

BoolReply Connection::del(const std::string& name)
{
    EXECUTE_COMMAND_SYNC1(Del, name);
    return BoolReply(this);
}

static std::string s_none = "none";
static std::string s_string = "string";
static std::string s_list = "list";
static std::string s_set = "set";
static std::string s_zset = "zset";
static std::string s_hash = "hash";

Type Connection::type(const std::string& name)
{
    EXECUTE_COMMAND_SYNC1(Type, name);
    std::string t = readStatusCodeReply();
    if(t == s_none)
        return None;
    if(t == s_string)
        return String;
    if(t == s_list)
        return List;
    if(t == s_set)
        return Set;
    if(t == s_zset)
        return ZSet;
    if(t == s_hash)
        return Hash;

    return None;
}

MultiBulkEnumerator Connection::keys(const std::string& pattern)
{
    EXECUTE_COMMAND_SYNC1(Keys, pattern);
    return MultiBulkEnumerator(this);
}

StringReply Connection::randomKey()
{
    EXECUTE_COMMAND_SYNC(RandomKey);
    return StringReply(this);
}

VoidReply Connection::rename(const std::string& oldName, const std::string& newName)
{
    EXECUTE_COMMAND_SYNC2(Rename, oldName, newName);
    return VoidReply(this);
}

BoolReply Connection::renameNX(const std::string& oldName, const std::string& newName)
{
    EXECUTE_COMMAND_SYNC2(RenameNX, oldName, newName);
    return BoolReply(this);
}

IntReply Connection::dbSize()
{
    EXECUTE_COMMAND_SYNC(DbSize);
    return IntReply(this);
}

BoolReply Connection::expire(const std::string& name, int seconds)
{
    EXECUTE_COMMAND_SYNC2(Expire, name, seconds);
    return BoolReply(this);
}

BoolReply Connection::expireAt(const std::string& name, int timestamp)
{
    EXECUTE_COMMAND_SYNC2(ExpireAt, name, timestamp);
    return BoolReply(this);
}

//TODO: persist

IntReply Connection::ttl(const std::string& name)
{
    EXECUTE_COMMAND_SYNC1(Ttl, name);
    return IntReply(this);
}

VoidReply Connection::select(int db)
{
    EXECUTE_COMMAND_SYNC1(Select, db);
    return VoidReply(this);
}

BoolReply Connection::move(const std::string& name, int db)
{
    EXECUTE_COMMAND_SYNC2(Move, name, db);
    return BoolReply(this);
}

VoidReply Connection::flushDb()
{
    EXECUTE_COMMAND_SYNC(FlushDb);
    return VoidReply(this);
}

VoidReply Connection::flushAll()
{
    EXECUTE_COMMAND_SYNC(FlushAll);
    return VoidReply(this);
}

VoidReply Connection::set(const std::string& name, const std::string& value)
{
    EXECUTE_COMMAND_SYNC2(Set, name, value);
    return VoidReply(this);
}

StringReply Connection::get(const std::string& name)
{
    EXECUTE_COMMAND_SYNC1(Get, name);
    return StringReply(this);
}

//TODO: mget

StringReply Connection::getSet(const std::string& name, const std::string& value)
{
    EXECUTE_COMMAND_SYNC2(GetSet, name, value);
    return StringReply(this);
}

BoolReply Connection::setNX(const std::string& name, const std::string& value)
{
    EXECUTE_COMMAND_SYNC2(SetNX, name, value);
    return BoolReply(this);
}

VoidReply Connection::setEx(const std::string& name, int time, const std::string& value)
{
    EXECUTE_COMMAND_SYNC3(SetEx, name, time, value);
    return VoidReply(this);
}

IntReply Connection::incr(const std::string& name)
{
    EXECUTE_COMMAND_SYNC1(Incr, name);
    return IntReply(this);
}

IntReply Connection::incrBy(const std::string& name, int value)
{
    EXECUTE_COMMAND_SYNC2(IncrBy, name, value);
    return IntReply(this);
}

IntReply Connection::decr(const std::string& name)
{
    EXECUTE_COMMAND_SYNC1(Decr, name);
    return IntReply(this);
}

IntReply Connection::decrBy(const std::string& name, int value)
{
    EXECUTE_COMMAND_SYNC2(DecrBy, name, value);
    return IntReply(this);
}

IntReply Connection::append(const std::string& name, const std::string& value)
{
    EXECUTE_COMMAND_SYNC2(Append, name, value);
    return IntReply(this);
}

StringReply Connection::subStr(const std::string& name, int start, int end)
{
    EXECUTE_COMMAND_SYNC3(SubStr, name, start, end);
    return StringReply(this);
}

IntReply Connection::rpush(const std::string& key, const std::string& value)
{
    EXECUTE_COMMAND_SYNC2(RPush, key, value);
    return IntReply(this);
}

IntReply Connection::lpush(const std::string& key, const std::string& value)
{
    EXECUTE_COMMAND_SYNC2(LPush, key, value);
    return IntReply(this);
}

IntReply Connection::llen(const std::string& key)
{
    EXECUTE_COMMAND_SYNC1(LLen, key);
    return IntReply(this);
}

MultiBulkEnumerator Connection::lrange(const std::string& key, int start, int end)
{
    EXECUTE_COMMAND_SYNC3(LRange, key, start, end);
    return MultiBulkEnumerator(this);
}

VoidReply Connection::ltrim(const std::string& key, int start, int end)
{
    EXECUTE_COMMAND_SYNC3(LTrim, key, start, end);
    return VoidReply(this);
}

StringReply Connection::lindex(const std::string& key, int index)
{
    EXECUTE_COMMAND_SYNC2(LIndex, key, index);
    return StringReply(this);
}

VoidReply Connection::lset(const std::string& key, int index, const std::string& value)
{
    EXECUTE_COMMAND_SYNC3(LSet, key, index, value);
    return VoidReply(this);
}

IntReply Connection::lrem(const std::string& key, int count, const std::string& value)
{
    EXECUTE_COMMAND_SYNC3(LRem, key, count, value);
    return IntReply(this);
}

StringReply Connection::lpop(const std::string& key)
{
    EXECUTE_COMMAND_SYNC1(LPop, key);
    return StringReply(this);
}

StringReply Connection::rpop(const std::string& key)
{
    EXECUTE_COMMAND_SYNC1(RPop, key);
    return StringReply(this);
}

//TODO: blpop
//TODO: brpop

StringReply Connection::rpopLpush(const std::string& src, const std::string& dest)
{
    EXECUTE_COMMAND_SYNC2(RPopLPush, src, dest);
    return StringReply(this);
}

BoolReply Connection::sadd(const std::string& key, const std::string& member)
{
    EXECUTE_COMMAND_SYNC2(SAdd, key, member);
    return BoolReply(this);
}

BoolReply Connection::srem(const std::string& key, const std::string& member)
{
    EXECUTE_COMMAND_SYNC2(SRem, key, member);
    return BoolReply(this);
}

StringReply Connection::spop(const std::string& key)
{
    EXECUTE_COMMAND_SYNC1(SPop, key);
    return StringReply(this);
}

BoolReply Connection::smove(const std::string& src, const std::string& dest, const std::string& member)
{
    EXECUTE_COMMAND_SYNC3(SMove, src, dest, member);
    return BoolReply(this);
}

IntReply Connection::scard(const std::string& key)
{
    EXECUTE_COMMAND_SYNC1(SCard, key);
    return IntReply(this);
}

BoolReply Connection::sisMember(const std::string& key, const std::string& member)
{
    EXECUTE_COMMAND_SYNC2(SIsMember, key, member);
    return BoolReply(this);
}

//TODO: sinter
//TODO: sinterstore
//TODO: sunion
//TODO: sunionstore
//TODO: sdiff
//TODO: sdiffstore

MultiBulkEnumerator Connection::smembers(const std::string& key)
{
    EXECUTE_COMMAND_SYNC1(SMembers, key);
    return MultiBulkEnumerator(this);
}

StringReply Connection::srandMember(const std::string& key)
{
    EXECUTE_COMMAND_SYNC1(SRandMember, key);
    return StringReply(this);
}

//TODO: all Z* functions

BoolReply Connection::hset(const std::string& key, const std::string& field, const std::string& value)
{
    EXECUTE_COMMAND_SYNC3(HSet, key, field, value);
    return BoolReply(this);
}

StringReply Connection::hget(const std::string& key, const std::string& field)
{
    EXECUTE_COMMAND_SYNC2(HGet, key, field);
    return StringReply(this);
}

BoolReply Connection::hsetNX(const std::string& key, const std::string& field, const std::string& value)
{
    EXECUTE_COMMAND_SYNC3(HSetNX, key, field, value);
    return BoolReply(this);
}

IntReply Connection::hincrBy(const std::string& key, const std::string& field, int value)
{
    EXECUTE_COMMAND_SYNC3(HIncrBy, key, field, value);
    return IntReply(this);
}

BoolReply Connection::hexists(const std::string& key, const std::string& field)
{
    EXECUTE_COMMAND_SYNC2(HExists, key, field);
    return BoolReply(this);
}

BoolReply Connection::hdel(const std::string& key, const std::string& field)
{
    EXECUTE_COMMAND_SYNC2(HDel, key, field);
    return BoolReply(this);
}

IntReply Connection::hlen(const std::string& key)
{
    EXECUTE_COMMAND_SYNC1(HLen, key);
    return IntReply(this);
}

MultiBulkEnumerator Connection::hkeys(const std::string& key)
{
    EXECUTE_COMMAND_SYNC1(HKeys, key);
    return MultiBulkEnumerator(this);
}

MultiBulkEnumerator Connection::hvals(const std::string& key)
{
    EXECUTE_COMMAND_SYNC1(HVals, key);
    return MultiBulkEnumerator(this);
}

MultiBulkEnumerator Connection::hgetAll(const std::string& key)
{
    EXECUTE_COMMAND_SYNC1(HGetAll, key);
    return MultiBulkEnumerator(this);
}

VoidReply Connection::save()
{
    EXECUTE_COMMAND_SYNC(Save);
    return VoidReply(this);
}

VoidReply Connection::bgSave()
{
    EXECUTE_COMMAND_SYNC(BgSave);
    return VoidReply(this);
}

VoidReply Connection::bgReWriteAOF()
{
    EXECUTE_COMMAND_SYNC(BgReWriteAOF);
    return VoidReply(this);
}

IntReply Connection::lastSave()
{
    EXECUTE_COMMAND_SYNC(LastSave);
    return IntReply(this);
}

void Connection::shutdown()
{
    EXECUTE_COMMAND_SYNC(Shutdown);
}

StringReply Connection::info()
{
    EXECUTE_COMMAND_SYNC(Info);
    return StringReply(this);
}

void Connection::subscribe(const std::string& channel)
{
    EXECUTE_COMMAND_SYNC1(Subscribe, channel);
}

void Connection::unsubscribe(const std::string& channel)
{
    EXECUTE_COMMAND_SYNC1(Unsubscribe, channel);
}

void Connection::psubscribe(const std::string& channel)
{
    EXECUTE_COMMAND_SYNC1(PSubscribe, channel);
}

void Connection::punsubscribe(const std::string& channel)
{
    EXECUTE_COMMAND_SYNC1(PUnsubscribe, channel);
}

IntReply Connection::publish(const std::string& channel, const std::string& message)
{
    EXECUTE_COMMAND_SYNC2(Publish, channel, message);
    return IntReply(this);
}

void Connection::multi()
{
    EXECUTE_COMMAND_SYNC(Multi);
}

void Connection::exec()
{
    EXECUTE_COMMAND_SYNC(Exec);
}

void Connection::discard()
{
    EXECUTE_COMMAND_SYNC(Discard);
}

Transaction::Transaction(Connection* conn)
: conn(conn), replies(conn)
{
    if(conn->transaction)
        throw std::runtime_error("cannot start a transaction while the connection is already in one");

    conn->multi();

    conn->transaction = this;
    replies.state = Dirty;
}

Transaction::~Transaction()
{
    try
    {
        abort();
    }
    catch(...)
    {}
    conn->transaction = NULL;
}

void Transaction::commit()
{
    if(replies.state == Dirty)
    {
        replies.state = Committed;
        conn->exec();
        replies.readResult();
    }
}

void Transaction::abort()
{
    if(replies.state == Dirty)
    {
        replies.state = Aborted;
        conn->discard();
        replies.readResult();
    }
}

void QueuedReply::readResult()
{
    if(!conn)
        return;

    Connection* const tmp = conn;
    conn = NULL;
    tmp->readStatusCodeReply();//one +OK for the MULTI
    //one +QUEUED per queued request (including this QueuedReply)
    for(size_t i = 0; i < count; ++i)
    {
        tmp->readStatusCodeReply();
    }
    if(state == Committed)
    {
        const int expectedCount = tmp->readIntegerReply();
        if(count != expectedCount)
            throw std::runtime_error("transaction item count did not match");
    }
    else if(state == Aborted)
    {
        tmp->readStatusCodeReply();
        //TODO: discard all remaining items to be parsed
    }
}

}; //namespace redispp

