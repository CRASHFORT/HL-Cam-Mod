#pragma once
#include "Shared\Binary Buffer\BinaryBuffer.hpp"

namespace Shared
{
	namespace Interprocess
	{
		namespace Config
		{
			using ByteType = unsigned char;
			using MessageType = unsigned char;

			enum
			{
				MaxMessageSize = 16384,
				MaxSimMessages = 50,
			};
		}

		class Peer
		{
		public:
			Peer();

			virtual ~Peer();

			/*
				Empty message.
			*/
			bool Write(Config::MessageType message);

			bool Write(Config::MessageType message, Utility::BinaryBuffer&& data);

			/*
				Waits if there is nothing to read, reserves size.
			*/
			bool ReadBlocking(Config::MessageType& message, Utility::BinaryBuffer& data);

			/*
				Returns false if there is nothing to read, reserves size.
			*/
			bool TryRead(Config::MessageType& message, Utility::BinaryBuffer& data);

		protected:
			std::string ConnectionPoint;
			std::unique_ptr<boost::interprocess::message_queue> MessageQueue;

		private:
			bool Write(const Utility::BinaryBuffer& data);
		};

		/*
			Connects to a shared memory object, drops reference on destruction.
		*/
		class Client final : public Peer
		{
		public:
			Client();
			Client(const std::string& connectionpoint);

			Client& operator=(Client&& other);
			
			void Connect(const std::string& connectionpoint);
			void Disconnect();
			bool IsConnected() const;

		private:
			
		};

		/*
			Owns a shared memory object, will remove on destruction.
		*/
		class Server final : public Peer
		{
		public:
			Server();
			Server(const std::string& connectionname);

			Server& operator=(Server&& other);

			~Server();

			void Start(const std::string& connectionname);
			void Stop();
			bool IsStarted() const;

		private:
			
		};
	}
}
