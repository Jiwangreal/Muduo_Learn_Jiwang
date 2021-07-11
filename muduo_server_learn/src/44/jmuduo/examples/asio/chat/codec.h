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
    // ������Ǳ�����
    // ������while������if����Ϊtcp���ֽ�����Ϣ�������յ����ǲ�ֹһ����Ϣ���������������Ϣ������
    while (buf->readableBytes() >= kHeaderLen) // kHeaderLen == 4
    {
      // ȡ����ͷ
      // FIXME: use Buffer::peekInt32()
      const void* data = buf->peek();//�������͵�����ݣ�����û�дӻ�����ȡ������
      // ��ȡ��ǰ4���ֽ�
      int32_t be32 = *static_cast<const int32_t*>(data); // SIGBUS
      const int32_t len = muduo::net::sockets::networkToHost32(be32);//���ת����
      if (len > 65536 || len < 0)
      {
        LOG_ERROR << "Invalid length " << len;
        conn->shutdown();  // FIXME: disable reading
        break;
      }
      else if (buf->readableBytes() >= len + kHeaderLen)  // �ﵽһ����������Ϣ
      {
        // �ӻ�����ȡ�߰�ͷ
        buf->retrieve(kHeaderLen);
        // ȡ������
        muduo::string message(buf->peek(), len);
        messageCallback_(conn, message, receiveTime);
        buf->retrieve(len);
      }
      // eg��0x00,0x00,0x00,0x05, 'h','e'�Ͳ�����������Ϣ���ᵽ����6>=4+5������
      /*���ͻ��˷���0x00,0x00,0x00,0x08, 'h','e','l','l','o'����������Ϣ�󣬾Ͳ��ٷ�����Ϣ�ˣ�����˾�break�ˣ��������ˣ��������δ���������Ϣ��
      ��1������1���������һ��Ӧ�ò��У����Ϣ�ģ�����˵CRC32У�飬У�������ô������Ϣ���Ǵ������Ϣ��

      0x00,0x00,0x00,0x08, 'h','e','l','l','o'����һ��������Ϣ����Ȼ����Ϊ8��������ҪCEC32У�飬�����Ƿ�����
      0x00,0x00,0x00,0x08, 'h','e','l','l','o',0x00,0x00�ⲻ��һ��������Ϣ

      ��2���ڶ��ַ�ʽ��
      ��������Ӧ���п��жϿ����ܣ���һ��ʱ��û���յ��ͻ��˵���Ϣ����Ӧ�öϿ�����
      ��Ϊ���������������⣩�ͻ��ˣ�����֮����ռ���������ӣ��������������������Ϣ�ˡ�
      */
      else  // δ�ﵽһ����������Ϣ�����ﴦ��ճ�����⣬TCP���ֽ�������Ӧ�ò����ޱ߽�ģ���Ҫ����������Ҫ�������
      {
        break;
      }
    }
  }

  // FIXME: TcpConnectionPtr
  void send(muduo::net::TcpConnection* conn,
            const muduo::StringPiece& message)
  {
    // ����
    muduo::net::Buffer buf;
    buf.append(message.data(), message.size());
    
    // ����
    int32_t len = static_cast<int32_t>(message.size());
    int32_t be32 = muduo::net::sockets::hostToNetwork32(len);

    // ��ͷ������4���ֽ�
    buf.prepend(&be32, sizeof be32);
    conn->send(&buf);
  }

 private:
  StringMessageCallback messageCallback_;
  const static size_t kHeaderLen = sizeof(int32_t);
};

#endif  // MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
