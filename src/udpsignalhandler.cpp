#include "udpsignalhandler.h"

#include <thread>
#include <sstream>

#ifdef _WIN32
#include <WS2tcpip.h>
#else
#include <netdb.h>
#endif

//debug
//#include <iostream>

UDPSignalHandler::UDPSignalHandler() : m_setpositionfunction(nullptr)
{
	WSAStartup(MAKEWORD(2, 2), &m_wsa);
}

UDPSignalHandler::~UDPSignalHandler()
{
	while (m_running)
	{
		Stop();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	WSACleanup();
}

bool UDPSignalHandler::IsRunning() const
{
	return m_running;
}

void UDPSignalHandler::Stop()
{
	m_stop = true;
}

void UDPSignalHandler::Start()
{
	if (m_running)
	{
		return;
	}

	m_running = true;
	m_stop = false;

	std::thread t = std::thread(&UDPSignalHandler::Run, this);
	t.detach();
}

void UDPSignalHandler::Run()
{
	m_running = true;
	m_stop = false;

	struct addrinfo hints, * res;
	struct sockaddr from;
	SOCKET sock;
	FD_SET fdset;
	struct timeval tv;
	char buff[32];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	getaddrinfo(NULL, "8050", &hints, &res);

	sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	bind(sock, res->ai_addr, res->ai_addrlen);

	//std::cout << "socket=" << sock << std::endl;

	freeaddrinfo(res);
	do
	{
		FD_ZERO(&fdset);
		FD_SET(sock, &fdset);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;		// 10ms
		int rval = select(sock + 1, &fdset, NULL, NULL, &tv);
		if (rval > 0)
		{
			if (FD_ISSET(sock, &fdset))
			{
				memset(buff, 0, 32);
				int fromlen = sizeof(struct sockaddr);
				int len = recvfrom(sock, buff, 31, 0, &from, &fromlen);
				if (len > 0)
				{
					buff[len] = '\0';
					//std::cout << "got " << buff << std::endl;

					double val = 0;
					std::istringstream istr((std::string(buff)));
					istr >> val;
					if (m_setpositionfunction)
					{
						m_setpositionfunction((val+0.5));
					}
				}
			}
		}
	} while (!m_stop);

#ifdef _WIN32
	closesocket(sock);
#else
	close(sock);
#endif

	m_stop = false;
	m_running = false;
}

void UDPSignalHandler::SetPositionFunction(std::function<void(int64_t)> func)
{
	m_setpositionfunction = func;
}
