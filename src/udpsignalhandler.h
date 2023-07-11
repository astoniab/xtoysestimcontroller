#pragma once

#include <atomic>
#include <string>
#include <functional>
#include <cstdint>

#ifdef _WIN32
#include <WinSock2.h>
#endif

class UDPSignalHandler
{
public:
	UDPSignalHandler();
	~UDPSignalHandler();

	bool IsRunning() const;
	void Stop();
	void Start();

	void SetPositionFunction(std::function<void(int64_t)> func);

private:

	std::atomic<bool> m_running;
	std::atomic<bool> m_stop;
	WSAData m_wsa;

	std::function<void(int64_t)> m_setpositionfunction;

	void Run();

};
