#pragma once

namespace Utility
{
	/*
		From TASView by Yalter:
		https://github.com/YaLTeR/TASView/blob/pipe/src/MessageQueue.hpp
		https://github.com/YaLTeR/TASView/blob/pipe/src/MessageQueue.cpp
	*/
	class MessageQueue
	{
	public:
		MessageQueue(const std::string& name);
		~MessageQueue();
		MessageQueue(const MessageQueue&) = delete;
		MessageQueue& operator=(const MessageQueue&) = delete;

		template<typename rep, typename period>
		bool timed_read(std::vector<unsigned char>& data, std::chrono::duration<rep, period> timeout, DWORD& bytesRead, HANDLE additionalEvent);

	private:
		HANDLE pipe;
		OVERLAPPED overlapped;
		bool waiting_for_completion;
	};

	template<typename rep, typename period>
	bool MessageQueue::timed_read(std::vector<unsigned char>& data, std::chrono::duration<rep, period> timeout, DWORD& bytes_read, HANDLE additionalEvent)
	{
		using namespace std;
		using namespace std::chrono;

		// If we aren't already waiting, start a new read.
		if (!waiting_for_completion)
		{
			if (ReadFile(pipe, data.data(), data.size(), &bytes_read, &overlapped))
			{
				// Read completed immediately.
				return true;
			}
			else
			{
				auto err = GetLastError();
				if (err == ERROR_IO_PENDING)
				{
					// Started reading.
					waiting_for_completion = true;
				}
				else
				{
					throw runtime_error("error reading a message: " + to_string(err));
				}
			}
		}

		// Waiting.
		assert(waiting_for_completion);
		const HANDLE handles[] = {additionalEvent, overlapped.hEvent};
		auto ret = WaitForMultipleObjects(2, handles, FALSE, static_cast<DWORD>(duration_cast<milliseconds>(timeout).count()));
		if (ret == WAIT_OBJECT_0)
		{
			// Time to exit the program.
			return false;
		}
		else if (ret == WAIT_TIMEOUT)
		{
			// Still waiting.
			return false;
		}
		else if (ret == WAIT_FAILED)
		{
			throw runtime_error("error waiting for objects: " + to_string(GetLastError()));
		}

		// If we got here then read must be completed.
		if (GetOverlappedResultEx(pipe, &overlapped, &bytes_read, 0, FALSE))
		{
			// Read completed.
			waiting_for_completion = false;
			return true;
		}
		else
		{
			auto err = GetLastError();
			if (err == ERROR_IO_INCOMPLETE)
			{
				// What?
				assert(0);
				return false;
			}
			else
			{
				throw runtime_error("error checking the message status: " + to_string(err));
			}
		}
	}

}
