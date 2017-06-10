#pragma once

#include <boost/foreach.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <iostream>
#include <list>
#include <memory>
#include <stdexcept>
#include <string.h>
#include <string>

namespace redispp {

class NullReplyException : std::out_of_range {
public:
  NullReplyException();
};

typedef std::pair<std::string, std::string> KeyValuePair;
typedef std::list<std::string> ArgList;
typedef std::list<KeyValuePair> KeyValueList;

template <typename BufferType> struct Command {
  Command(const char* name) : cmdName(std::string(name)), numArgs(0) {}

  virtual ~Command() {}

  std::string header() {
    std::string header;

    header = "*";
    header += boost::lexical_cast<std::string>(numArgs + 1);
    header += "\r\n";
    header += "$";
    header += boost::lexical_cast<std::string>(cmdName.length());
    header += "\r\n";
    header += cmdName;
    header += "\r\n";

    return header;
  }

  void execute(BufferType const& dest) { dest->write(header()); }

  template <typename T> void execute(T const& arg1, BufferType const& dest) {
    numArgs = 1;
    dest->write(header());
    dest->writeArg(arg1);
  }

  template <typename T1, typename T2>
  void execute(T1 const& arg1, T2 const& arg2, BufferType const& dest) {
    numArgs = 2;
    dest->write(header());
    dest->writeArg(arg1);
    dest->writeArg(arg2);
  }

  template <typename T1, typename T2, typename T3>
  void execute(T1 const& arg1, T2 const& arg2, T3 const& arg3,
               BufferType const& dest) {
    numArgs = 3;
    dest->write(header());
    dest->writeArg(arg1);
    dest->writeArg(arg2);
    dest->writeArg(arg3);
  }

  void execute(const ArgList& args, const BufferType& dest) {
    numArgs = args.size();
    dest->write(header());

    BOOST_FOREACH (std::string arg, args) { dest->writeArg(arg); }
  }

  void execute(const std::string& arg1, const ArgList& args,
               const BufferType& dest) {
    numArgs = args.size() + 1;
    dest->write(header());
    dest->writeArg(arg1);

    BOOST_FOREACH (std::string arg, args) { dest->writeArg(arg); }
  }

  void execute(const std::string& arg1, const KeyValueList& args,
               const BufferType& dest) {
    numArgs = args.size() * 2 + 1;
    dest->write(header());
    dest->writeArg(arg1);

    BOOST_FOREACH (KeyValuePair arg, args) {
      dest->writeArg(arg.first);
      dest->writeArg(arg.second);
    }
  }

  void execute(const ArgList& args, const int argn, const BufferType& dest) {
    numArgs = args.size() + 1;
    dest->write(header());

    BOOST_FOREACH (std::string arg, args) { dest->writeArg(arg); }
    dest->writeArg(argn);
  }

  void execute(const std::string& arg1, const ArgList& argList1,
               const ArgList& argList2, const BufferType& dest) {
    numArgs = argList1.size() + argList2.size() + 2;
    dest->write(header());
    dest->writeArg(arg1);
    dest->writeArg(argList1.size());

    BOOST_FOREACH (std::string arg, argList1) { dest->writeArg(arg); }

    BOOST_FOREACH (std::string arg, argList2) { dest->writeArg(arg); }
  }

  std::string cmdName;
  int numArgs;
};

#define DEFINE_COMMAND(name, args)                                             \
  struct name##Command : public Command<std::unique_ptr<Buffer>> {             \
    name##Command() : Command<std::unique_ptr<Buffer>>(#name) {}               \
  };                                                                           \
  name##Command _##name##Command;

enum Type {
  None,
  String,
  List,
  Set,
  ZSet,
  Hash,
};

class Connection;
class ClientSocket;
class Buffer;

typedef boost::intrusive::list_base_hook<
    boost::intrusive::link_mode<boost::intrusive::auto_unlink>>
    auto_unlink_hook;

class BaseReply : public auto_unlink_hook {
  friend class Connection;

public:
  BaseReply() : conn(NULL) {}

  BaseReply(const BaseReply& other);

  BaseReply& operator=(const BaseReply& other);

  virtual ~BaseReply() {}

protected:
  virtual void readResult() = 0;

  void clearPendingResults();

  BaseReply(Connection* conn);

  mutable Connection* conn;
};

typedef boost::intrusive::list<BaseReply,
                               boost::intrusive::constant_time_size<false>>
    ReplyList;

class Transaction;

enum TransactionState {
  Blank,
  Dirty,
  Aborted,
  Committed,
};

class QueuedReply : public BaseReply {
  friend class BaseReply;
  friend class Connection;
  friend class Transaction;

public:
  QueuedReply() : count(0), state(Blank) {}

  ~QueuedReply() {}

protected:
  virtual void readResult();

private:
  QueuedReply(Connection* conn) : BaseReply(conn), count(0), state(Blank) {}

  size_t count;
  TransactionState state;
};

class VoidReply : public BaseReply {
  friend class Connection;

public:
  VoidReply() : storedResult(false) {}

  ~VoidReply();

  VoidReply(const VoidReply& other)
      : BaseReply(other), storedResult(other.storedResult) {}

  VoidReply& operator=(const VoidReply& other) {
    result();
    BaseReply::operator=(other);
    storedResult = other.storedResult;
    return *this;
  }

  bool result();

  operator bool() { return result(); }

protected:
  virtual void readResult() { result(); }

private:
  VoidReply(Connection* conn);

  bool storedResult;
};

class BoolReply : public BaseReply {
  friend class Connection;

public:
  BoolReply() : storedResult(false) {}

  ~BoolReply();

  BoolReply(const BoolReply& other)
      : BaseReply(other), storedResult(other.storedResult) {}

  BoolReply& operator=(const BoolReply& other) {
    result();
    BaseReply::operator=(other);
    storedResult = other.storedResult;
    return *this;
  }

  bool result();

  operator bool() { return result(); }

protected:
  virtual void readResult() { result(); }

private:
  BoolReply(Connection* conn);

  bool storedResult;
};

class IntReply : public BaseReply {
  friend class Connection;

public:
  IntReply() : storedResult(0) {}

  ~IntReply();

  IntReply(const IntReply& other)
      : BaseReply(other), storedResult(other.storedResult) {}

  IntReply& operator=(const IntReply& other) {
    result();
    BaseReply::operator=(other);
    storedResult = other.storedResult;
    return *this;
  }

  int64_t result();

  operator int() { return result(); }

protected:
  virtual void readResult() { result(); }

private:
  IntReply(Connection* conn);

  int64_t storedResult;
};

class StringReply : public BaseReply {
  friend class Connection;

public:
  StringReply() {}

  ~StringReply();

  StringReply(const StringReply& other)
      : BaseReply(other), storedResult(other.storedResult) {}

  StringReply& operator=(const StringReply& other) {
    result();
    BaseReply::operator=(other);
    storedResult = other.storedResult;
    return *this;
  }

  const boost::optional<std::string>& result();

  operator std::string() {
    result();
    if (!storedResult) {
      throw NullReplyException();
    }
    return *storedResult;
  }

protected:
  virtual void readResult() { result(); }

private:
  StringReply(Connection* conn);

  boost::optional<std::string> storedResult;
};

class MultiBulkEnumerator : public BaseReply {
  friend class Connection;

public:
  MultiBulkEnumerator() : headerDone(false), count(0) {}

  ~MultiBulkEnumerator();

  MultiBulkEnumerator(const MultiBulkEnumerator& other)
      : BaseReply(other), headerDone(other.headerDone), count(other.count) {
    pending.splice(pending.begin(), other.pending);
  }

  MultiBulkEnumerator& operator=(const MultiBulkEnumerator& other) {
    if (conn && count > 0) {
      // assume unread data can be discarded, this is the only object that
      // could/would have read it
      std::string tmp;
      while (next(&tmp))
        ;
    }
    pending.clear();
    BaseReply::operator=(other);
    headerDone = other.headerDone;
    count = other.count;
    return *this;
  }

  bool nextOptional(boost::optional<std::string>& out);
  bool next(std::string* out);

protected:
  virtual void readResult() {
    if (conn && (!headerDone || count > 0)) {
      std::list<boost::optional<std::string>> readPending;
      boost::optional<std::string> tmp;
      while (nextOptional(tmp)) {
        readPending.push_back(tmp);
      }
      pending.splice(pending.end(), readPending);
    }
  }

  MultiBulkEnumerator(Connection* conn);

  bool headerDone;
  int count;
  mutable std::list<boost::optional<std::string>> pending;
};

class Connection;

class Transaction : boost::noncopyable {
  friend class BaseReply;

public:
  Transaction(Connection* conn);

  ~Transaction();

  void commit();

  void abort();

private:
  Connection* conn;
  QueuedReply replies;
};

class Connection {
  friend class BaseReply;
  friend class QueuedReply;
  friend class VoidReply;
  friend class BoolReply;
  friend class IntReply;
  friend class StringReply;
  friend class MultiBulkEnumerator;
  friend class Transaction;

public:
  static const size_t kDefaultBufferSize = 4 * 1024;

  Connection(const std::string& host, const std::string& port,
             const std::string& password, bool noDelay = false,
             size_t bufferSize = kDefaultBufferSize);
#ifndef _WIN32
  Connection(const std::string& unixDomainSocket, const std::string& password,
             size_t bufferSize = kDefaultBufferSize);
#endif

  ~Connection();

  void quit();

  VoidReply authenticate(const char* password);

  BoolReply exists(const std::string& name);
  BoolReply del(const std::string& name);

  Type type(const std::string& name);

  MultiBulkEnumerator keys(const std::string& pattern);
  StringReply randomKey();

  VoidReply rename(const std::string& oldName, const std::string& newName);
  BoolReply renameNX(const std::string& oldName, const std::string& newName);

  IntReply dbSize();

  BoolReply expire(const std::string& name, int seconds);
  BoolReply expireAt(const std::string& name, int timestamp);
  // TODO: persist
  IntReply ttl(const std::string& name);

  VoidReply select(int db);
  BoolReply move(const std::string& name, int db);

  VoidReply flushDb();
  VoidReply flushAll();

  VoidReply set(const std::string& name, const std::string& value);
  StringReply get(const std::string& name);
  // TODO: mget
  StringReply getSet(const std::string& name, const std::string& value);
  BoolReply setNX(const std::string& name, const std::string& value);
  VoidReply setEx(const std::string& name, int time, const std::string& value);

  // TODO: mset
  // TODO: msetnx

  IntReply incr(const std::string& name);
  IntReply incrBy(const std::string& name, int value);

  IntReply decr(const std::string& name);
  IntReply decrBy(const std::string& name, int value);

  IntReply append(const std::string& name, const std::string& value);
  StringReply subStr(const std::string& name, int start, int end);

  IntReply rpush(const std::string& key, const std::string& value);
  IntReply lpush(const std::string& key, const std::string& value);
  IntReply llen(const std::string& key);
  MultiBulkEnumerator lrange(const std::string& key, int start, int end);
  VoidReply ltrim(const std::string& key, int start, int end);
  StringReply lindex(const std::string& key, int index);
  VoidReply lset(const std::string& key, int index, const std::string& value);
  IntReply lrem(const std::string& key, int count, const std::string& value);
  StringReply lpop(const std::string& key);
  StringReply rpop(const std::string& key);
  MultiBulkEnumerator blpop(ArgList keys, int timeout);
  MultiBulkEnumerator brpop(ArgList keys, int timeout);
  StringReply rpopLpush(const std::string& src, const std::string& dest);

  BoolReply sadd(const std::string& key, const std::string& member);
  BoolReply srem(const std::string& key, const std::string& member);
  StringReply spop(const std::string& key);
  BoolReply smove(const std::string& src, const std::string& dest,
                  const std::string& member);
  IntReply scard(const std::string& key);
  BoolReply sisMember(const std::string& key, const std::string& member);
  MultiBulkEnumerator sinter(const ArgList& keys);
  IntReply sinterStore(const std::string& key, const ArgList& keys);
  MultiBulkEnumerator sunion(const ArgList& keys);
  IntReply sunionStore(const std::string& key, const ArgList& keys);
  MultiBulkEnumerator sdiff(const ArgList& keys);
  IntReply sdiffStore(const std::string& key, const ArgList& keys);
  MultiBulkEnumerator smembers(const std::string& key);
  StringReply srandMember(const std::string& key);

  // TODO: all Z* functions

  BoolReply hset(const std::string& key, const std::string& field,
                 const std::string& value);
  StringReply hget(const std::string& key, const std::string& field);
  BoolReply hsetNX(const std::string& key, const std::string& field,
                   const std::string& value);
  MultiBulkEnumerator hmget(const std::string& key,
                            const std::list<std::string>& fields);
  VoidReply hmset(const std::string& key,
                  const std::list<std::pair<std::string, std::string>>& fields);
  IntReply hincrBy(const std::string& key, const std::string& field, int value);
  BoolReply hexists(const std::string& key, const std::string& field);
  BoolReply hdel(const std::string& key, const std::string& field);
  IntReply hlen(const std::string& key);
  MultiBulkEnumerator hkeys(const std::string& key);
  MultiBulkEnumerator hvals(const std::string& key);
  MultiBulkEnumerator hgetAll(const std::string& key);

  MultiBulkEnumerator scriptExists(const ArgList& script);
  VoidReply scriptFlush();
  VoidReply scriptKill();
  StringReply scriptLoad(const std::string& script);

  MultiBulkEnumerator eval(const std::string& script, const ArgList& keys,
                           const ArgList& args);
  MultiBulkEnumerator evalSha(const std::string& sha, const ArgList& keys,
                              const ArgList& args);

  VoidReply save();
  VoidReply bgSave();
  VoidReply bgReWriteAOF();
  IntReply lastSave();
  void shutdown();
  StringReply info();

  void subscribe(const std::string& channel);
  void unsubscribe(const std::string& channel);
  void psubscribe(const std::string& channel);
  void punsubscribe(const std::string& channel);
  IntReply publish(const std::string& channel, const std::string& message);

private:
  char statusCode();
  void readErrorReply();
  void readStatusCodeReply(std::string* out);
  std::string readStatusCodeReply();
  int64_t readIntegerReply();
  void readBulkReply(boost::optional<std::string>& out);
  boost::optional<std::string> readBulkReply();

  std::unique_ptr<ClientSocket> connection;
  std::unique_ptr<std::iostream> ioStream;
  std::unique_ptr<Buffer> buffer;
  ReplyList outstandingReplies;
  Transaction* transaction;

  DEFINE_COMMAND(Quit, 0);
  DEFINE_COMMAND(Auth, 1);
  DEFINE_COMMAND(Exists, 1);
  DEFINE_COMMAND(Del, 1);
  DEFINE_COMMAND(Type, 1);
  DEFINE_COMMAND(Keys, 1);
  DEFINE_COMMAND(RandomKey, 0);
  DEFINE_COMMAND(Rename, 2);
  DEFINE_COMMAND(RenameNX, 2);
  DEFINE_COMMAND(DbSize, 0);
  DEFINE_COMMAND(Expire, 2);
  DEFINE_COMMAND(ExpireAt, 2);
  DEFINE_COMMAND(Persist, 1);
  DEFINE_COMMAND(Ttl, 1);
  DEFINE_COMMAND(Select, 1);
  DEFINE_COMMAND(Move, 2);
  DEFINE_COMMAND(FlushDb, 0);
  DEFINE_COMMAND(FlushAll, 0);
  DEFINE_COMMAND(Set, 2);
  DEFINE_COMMAND(Get, 1);
  DEFINE_COMMAND(GetSet, 2);
  DEFINE_COMMAND(SetNX, 2);
  DEFINE_COMMAND(SetEx, 3);
  DEFINE_COMMAND(Incr, 1);
  DEFINE_COMMAND(IncrBy, 2);
  DEFINE_COMMAND(Decr, 1);
  DEFINE_COMMAND(DecrBy, 2);
  DEFINE_COMMAND(Append, 2);
  DEFINE_COMMAND(SubStr, 3);

  DEFINE_COMMAND(RPush, 2);
  DEFINE_COMMAND(LPush, 2);
  DEFINE_COMMAND(LLen, 1);
  DEFINE_COMMAND(LRange, 3);
  DEFINE_COMMAND(LTrim, 3);
  DEFINE_COMMAND(LIndex, 2);
  DEFINE_COMMAND(LSet, 3);
  DEFINE_COMMAND(LRem, 3);
  DEFINE_COMMAND(LPop, 1);
  DEFINE_COMMAND(RPop, 1);
  DEFINE_COMMAND(BLPop, 2);
  DEFINE_COMMAND(BRPop, 2);
  DEFINE_COMMAND(RPopLPush, 2);
  // TODO: sort

  DEFINE_COMMAND(SAdd, 2);
  DEFINE_COMMAND(SRem, 2);
  DEFINE_COMMAND(SPop, 1);
  DEFINE_COMMAND(SMove, 3);
  DEFINE_COMMAND(SCard, 1);
  DEFINE_COMMAND(SIsMember, 2);
  DEFINE_COMMAND(SInter, 1);
  DEFINE_COMMAND(SInterStore, 2);
  DEFINE_COMMAND(SUnion, 1);
  DEFINE_COMMAND(SUnionStore, 2);
  DEFINE_COMMAND(SDiff, 1);
  DEFINE_COMMAND(SDiffStore, 2);
  DEFINE_COMMAND(SMembers, 1);
  DEFINE_COMMAND(SRandMember, 1);

  DEFINE_COMMAND(ZAdd, 2);
  DEFINE_COMMAND(ZRem, 2);
  DEFINE_COMMAND(ZIncrBy, 3);
  DEFINE_COMMAND(ZRank, 2);
  DEFINE_COMMAND(ZRevRank, 2);
  DEFINE_COMMAND(ZRange, 3);
  DEFINE_COMMAND(ZRevRange, 3);
  DEFINE_COMMAND(ZRangeByScore, 3);
  DEFINE_COMMAND(ZCount, 3);
  DEFINE_COMMAND(ZRemRangeByRank, 3);
  DEFINE_COMMAND(ZRemRangeByScore, 3);
  DEFINE_COMMAND(ZCard, 1);
  DEFINE_COMMAND(ZScore, 2);
  // TODO: zunionstore
  // TODO: zinterstore

  DEFINE_COMMAND(HSet, 3);
  DEFINE_COMMAND(HSetNX, 3);
  DEFINE_COMMAND(HGet, 2);
  DEFINE_COMMAND(HMGet, 2);
  DEFINE_COMMAND(HMSet, 2);
  DEFINE_COMMAND(HIncrBy, 3);
  DEFINE_COMMAND(HExists, 2);
  DEFINE_COMMAND(HDel, 2);
  DEFINE_COMMAND(HLen, 1);
  DEFINE_COMMAND(HKeys, 1);
  DEFINE_COMMAND(HVals, 1);
  DEFINE_COMMAND(HGetAll, 1);

  DEFINE_COMMAND(Script, 2);
  DEFINE_COMMAND(Eval, 3);
  DEFINE_COMMAND(EvalSha, 3);

  DEFINE_COMMAND(Save, 0);
  DEFINE_COMMAND(BgSave, 0);
  DEFINE_COMMAND(LastSave, 0);
  DEFINE_COMMAND(Shutdown, 0);
  DEFINE_COMMAND(BgReWriteAOF, 0);
  DEFINE_COMMAND(Info, 0);

  DEFINE_COMMAND(Subscribe, 1);
  DEFINE_COMMAND(Unsubscribe, 1);
  DEFINE_COMMAND(PSubscribe, 1);
  DEFINE_COMMAND(PUnsubscribe, 1);
  DEFINE_COMMAND(Publish, 2);

  // TODO: watch
  // TODO: unwatch

  DEFINE_COMMAND(Multi, 0);
  DEFINE_COMMAND(Exec, 0);
  DEFINE_COMMAND(Discard, 0);

  void multi();
  void exec();
  void discard();
};
};
