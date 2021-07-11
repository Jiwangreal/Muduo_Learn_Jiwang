#ifndef MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
#define MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H

#include <muduo/base/Logging.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/Endian.h>
#include <muduo/net/TcpConnection.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

class LengthHeaderCodec : boost::noncopyable
{
 public:
  typedef boost::function<void (const muduo::net::TcpConnectionPtr&,
                                const muduo::string& message,
                                muduo::Timestamp)> StringMessageCallback;

  explicit LengthHeaderCodec(const StringMessageCallback& cb)
    : messageCallback_(cb)
  {
  }

  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp receiveTime)
  {
    // 下面就是编解码的
    // 这里用while而不用if，因为tcp是字节流消息，可能收到的是不止一条消息，还需继续解析消息并处理
    while (buf->readableBytes() >= kHeaderLen) // kHeaderLen == 4
    {
      // 取出包头
      // FIXME: use Buffer::peekInt32()
      const void* data = buf->peek();//这里仅是偷看数据！！并没有从缓冲区取出数据
      // 先取出前4个字节
      int32_t be32 = *static_cast<const int32_t*>(data); // SIGBUS
      const int32_t len = muduo::net::sockets::networkToHost32(be32);//大端转主机
      if (len > 65536 || len < 0)
      {
        LOG_ERROR << "Invalid length " << len;
        conn->shutdown();  // FIXME: disable reading
        break;
      }
      else if (buf->readableBytes() >= len + kHeaderLen)  // 达到一条完整的消息
      {
        // 从缓冲区取走包头
        buf->retrieve(kHeaderLen);
        // 取出包体
        muduo::string message(buf->peek(), len);
        messageCallback_(conn, message, receiveTime);
        buf->retrieve(len);
      }
      // eg：0x00,0x00,0x00,0x05, 'h','e'就不是完整的消息，会到这里6>=4+5不满足
      /*若客户端发送0x00,0x00,0x00,0x08, 'h','e','l','l','o'这条错误消息后，就不再发送消息了，服务端就break了，就阻塞了，服务端如何处理错误的消息？
      （1）方法1：还会带上一个应用层的校验消息的，比如说CRC32校验，校验错误，那么这条消息就是错误的消息。

      0x00,0x00,0x00,0x08, 'h','e','l','l','o'这是一条完整消息；虽然包体为8，所以需要CEC32校验，看包是否完整
      0x00,0x00,0x00,0x08, 'h','e','l','l','o',0x00,0x00这不是一条完整消息

      （2）第二种方式：
      服务器端应该有空闲断开功能，在一定时间没有收到客户端的消息，就应该断开他。
      因为可能是正常（恶意）客户端，连接之后但是占用这条连接，后续不向服务器发送消息了。
      */
      else  // 未达到一条完整的消息，这里处理粘包问题，TCP是字节流，在应用层是无边界的，需要处理，所以需要编解码器
      {
        break;
      }
    }
  }

  // FIXME: TcpConnectionPtr
  void send(muduo::net::TcpConnection* conn,
            const muduo::StringPiece& message)
  {
    // 数据
    muduo::net::Buffer buf;
    buf.append(message.data(), message.size());
    
    // 长度
    int32_t len = static_cast<int32_t>(message.size());
    int32_t be32 = muduo::net::sockets::hostToNetwork32(len);

    // 在头部增加4个字节
    buf.prepend(&be32, sizeof be32);
    conn->send(&buf);
  }

 private:
  StringMessageCallback messageCallback_;
  const static size_t kHeaderLen = sizeof(int32_t);
};

#endif  // MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
