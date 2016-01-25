#include <vector>
#include <string>
#include <memory>
#include "boost\interprocess\ipc\message_queue.hpp"

#include "Shared\Interprocess\Interprocess.hpp"

namespace Shared
{
	namespace Interprocess
	{
		Peer::Peer()
		{

		}

		Peer::~Peer()
		{

		}

		bool Peer::Write(const Utility::BinaryBuffer& data)
		{
			if (!MessageQueue)
			{
				return false;
			}

			auto res = MessageQueue->try_send(data.GetData(), data.GetSize(), 0);

			return res;
		}

		bool Peer::Write(Config::MessageType message)
		{
			if (!MessageQueue)
			{
				return false;
			}

			return Write(message, {});
		}

		bool Peer::Write(Config::MessageType message, Utility::BinaryBuffer&& data)
		{
			if (!MessageQueue)
			{
				return false;
			}

			Utility::BinaryBuffer alldata;
			alldata << message;

			if (data.GetSize() != 0)
			{
				alldata.Append(std::move(data));
			}

			return Write(alldata);
		}

		bool Peer::ReadBlocking(Config::MessageType& message, Utility::BinaryBuffer& data)
		{
			if (!MessageQueue)
			{
				return false;
			}

			data.Resize(Config::MaxMessageSize);

			size_t received;
			size_t priority;

			MessageQueue->receive(data.GetDataModify(), Config::MaxMessageSize, received, priority);

			auto res = received > 0;

			if (res)
			{
				data >> message;
				data.Resize(received);
			}

			return res;
		}

		bool Peer::TryRead(Config::MessageType& message, Utility::BinaryBuffer& data)
		{
			if (!MessageQueue)
			{
				return false;
			}

			data.Resize(Config::MaxMessageSize);

			size_t received;
			size_t priority;

			auto res = MessageQueue->try_receive(data.GetDataModify(), Config::MaxMessageSize, received, priority);

			if (res)
			{
				data >> message;
				data.Resize(received);
			}

			return res;
		}

		Client::Client()
		{

		}

		Client::Client(const std::string& connectionpoint)
		{
			Connect(connectionpoint);
		}

		Client& Client::operator=(Client&& other)
		{
			if (other.MessageQueue)
			{
				MessageQueue = std::move(other.MessageQueue);
			}

			ConnectionPoint = std::move(other.ConnectionPoint);

			return *this;
		}

		void Client::Connect(const std::string& connectionpoint)
		{
			using namespace boost::interprocess;

			ConnectionPoint = connectionpoint;

			MessageQueue = std::make_unique<message_queue>
			(
				open_only,
				connectionpoint.c_str()
			);
		}

		void Client::Disconnect()
		{
			MessageQueue.reset();
		}

		bool Client::IsConnected() const
		{
			return MessageQueue != nullptr;
		}

		Server::Server()
		{

		}

		Server::Server(const std::string& connectionname)
		{
			Start(connectionname);
		}

		Server& Server::operator=(Server&& other)
		{
			if (other.MessageQueue)
			{
				MessageQueue = std::move(other.MessageQueue);
			}

			ConnectionPoint = std::move(other.ConnectionPoint);

			return *this;
		}

		Server::~Server()
		{
			Stop();
		}

		void Server::Start(const std::string& connectionname)
		{
			using namespace boost::interprocess;

			ConnectionPoint = connectionname;

			MessageQueue = std::make_unique<message_queue>
			(
				create_only,
				connectionname.c_str(),
				Config::MaxSimMessages,
				Config::MaxMessageSize
			);
		}

		void Server::Stop()
		{
			auto res = boost::interprocess::message_queue::remove(ConnectionPoint.c_str());
			MessageQueue.reset();
		}

		bool Server::IsStarted() const
		{
			return MessageQueue != nullptr;
		}
	}
}
