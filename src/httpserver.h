#pragma once

#include <atomic>
#include <string>
#include <functional>
#include <vector>

#include "mongoose.h"

class HTTPServer
{
public:
	HTTPServer();
	~HTTPServer();

	bool IsRunning() const;
	void Stop();
	void Start(const std::string& host, const std::string& port);

	void SetControlFunction(std::function<void(std::vector<std::pair<std::string, std::string>>&, std::string &)> func);
	void SetWebhookFunction(std::function<void(std::vector<std::pair<std::string, std::string>>&)> func);

private:

	std::atomic<bool> m_running;
	std::atomic<bool> m_stop;
	mg_mgr m_mgr;

	std::function<void(std::vector<std::pair<std::string, std::string>>&, std::string &)> m_controlfunction;
	std::function<void(std::vector<std::pair<std::string, std::string>>&)> m_webhookfunction;

	void Run(const std::string listenaddress);
	static void EventHandler(mg_connection* conn, int ev, void* ev_data, void* user_data);

	bool GetQueryVar(mg_str *uri, const std::string& var, std::vector<std::pair<std::string, std::string>>& params);

};
