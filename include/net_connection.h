#ifndef NET_CONNECTION_H
#define NET_CONNECTION_H


#include "net_common.h"
#include "net_tsqueue.h"

namespace net
{
constexpr uint BUFFER_SIZE = 20 * 1024;

class connection
{
public:
	connection(asio::io_context& context, asio::ip::tcp::socket socket, tsqueue<std::pair<std::string,std::string>>& messages)
		: m_Context(context), m_Socket(std::move(socket)), m_Messages(messages), m_Buffer(BUFFER_SIZE) {}

	void Connect(asio::ip::tcp::resolver::results_type& endpoints)
	{
		PROFILE_FUNCTION();
		asio::async_connect(m_Socket, endpoints,
			[this](std::error_code ec, asio::ip::tcp::endpoint endpoints)
			{
				if (!ec)
					Read();
				else
					std::cerr << "[net_connection][Connect]: " << ec.message() << '\n';
			}
		);
	}

	void Disconnect()
	{
		PROFILE_FUNCTION();
		if (IsConnected()) 
			asio::post(m_Context, [this](){ m_Socket.close(); });
	}

	bool IsConnected() const
	{
		return m_Socket.is_open();
	}

	void Send(std::string_view msg)
	{
		PROFILE_FUNCTION();
		std::cout << ">> " << msg << '\n';
		m_Socket.write_some(asio::buffer(msg.data(), msg.size()));
	}
	
	void Read()
	{
		PROFILE_FUNCTION();
		m_Socket.async_read_some(asio::buffer(m_Buffer.data(), m_Buffer.size()),
			[&](std::error_code ec, std::size_t len)
			{
				if (!ec)
				{
					std::string msg(m_Buffer.data(), len);
					(msg.rfind("PING ", 0) == 0) ? Send("PONG :tmi.twitch.tv\r\n") : AddToMessageQueue(msg);
					Read();
				}
				else
					std::cerr << "[net_connection]:[Read] " << ec.message() << '\n';
			}
		);
	}

	void AddToMessageQueue(const std::string& msg)
	{
		PROFILE_FUNCTION();
		std::smatch match;
		std::regex_search(msg, match, m_Regex);
		m_Messages.push_back(std::make_pair<std::string, std::string>(std::move(match[1]),std::move(match[3])));
	}

protected:
	asio::io_context& m_Context;
	asio::ip::tcp::socket m_Socket;
	tsqueue<std::pair<std::string,std::string>>& m_Messages;
	std::vector<char> m_Buffer;

private:
	const std::regex m_Regex {"!(.+)@.+ PRIVMSG #([^\\s]+) :(.*)"};
};
}


#endif // NET_CONNECTION_H