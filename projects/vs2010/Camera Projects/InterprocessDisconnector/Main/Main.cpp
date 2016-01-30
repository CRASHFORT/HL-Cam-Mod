#include <iostream>
#include "boost\interprocess\ipc\message_queue.hpp"

int wmain(int argc, wchar_t* argv[])
{
	auto gameres = boost::interprocess::message_queue::remove("HLCAM_GAME");

	if (gameres)
	{
		std::cout << "Removed HLCAM_GAME" << std::endl;
	}

	else
	{
		std::cout << "Could not remove HLCAM_GAME" << std::endl;
	}

	auto appres = boost::interprocess::message_queue::remove("HLCAM_APP");

	if (appres)
	{
		std::cout << "Removed HLCAM_APP" << std::endl;
	}

	else
	{
		std::cout << "Could not remove HLCAM_APP" << std::endl;
	}

	std::cout << "Done" << std::endl;

	std::cin.get();
}
