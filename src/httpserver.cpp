#include "httpserver.h"

#include <thread>
#include <chrono>
#include <vector>

//debug
//#include <iostream>

HTTPServer::HTTPServer() :m_running(false), m_stop(false), m_controlfunction(nullptr), m_webhookfunction(nullptr)
{
	mg_mgr_init(&m_mgr);
}

HTTPServer::~HTTPServer()
{
	while (m_running)
	{
		Stop();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	mg_mgr_free(&m_mgr);
}

bool HTTPServer::IsRunning() const
{
	return m_running;
}

void HTTPServer::Stop()
{
	m_stop = true;
}

void HTTPServer::Start(const std::string& host, const std::string& port)
{
	if (m_running)
	{
		return;
	}

	m_running = true;
	m_stop = false;
	std::string listenaddress = "http://" + host + ":" + port;
	std::thread t = std::thread(&HTTPServer::Run, this, listenaddress);
	t.detach();
}

void HTTPServer::Run(const std::string listenaddress)
{
	m_running = true;
	m_stop = false;

	mg_http_listen(&m_mgr, listenaddress.c_str(), &HTTPServer::EventHandler, this);

	do
	{
		mg_mgr_poll(&m_mgr, 1000);
	} while (!m_stop);

	m_stop = false;
	m_running = false;
}

void HTTPServer::EventHandler(mg_connection* conn, int ev, void* ev_data, void* user_data)
{
	HTTPServer &t = *(HTTPServer*)user_data;
	if (ev == MG_EV_HTTP_MSG)
	{
		mg_http_message* m = (mg_http_message*)ev_data;
		std::string uri(m->uri.ptr, m->uri.len);
		//std::cout << "Request " << uri << std::endl;

		if (mg_http_match_uri(m, "/") || mg_http_match_uri(m, "/index.*"))
		{
			mg_http_serve_opts opts;
			memset(&opts, 0, sizeof(mg_http_serve_opts));
			mg_http_serve_file(conn, m, "index.html", &opts);
		}
		/*
		else if (mg_http_match_uri(m, "/help.*"))
		{
			mg_http_serve_opts opts;
			memset(&opts, 0, sizeof(mg_http_serve_opts));
			mg_http_serve_file(conn, m, "help.html", &opts);
		}
		*/
		else if (mg_http_match_uri(m, "/help"))
		{
			std::string headers("Content-Type: text/plain\r\n");

			std::string body("");
			body += "Call server with query parameters";
			body += "/control?param1=value1&param2=value2\r\n";
			body += "\taction\t(stop|addstrokes|strokeconfig|setvolume|setmode)\r\n";
			body += "\r\nstrokeconfig:\r\n";
			body += "\tfrequency\r\n";
			body += "\tspm\r\n";
			body += "\tminpos\r\n";
			body += "\tmaxpos\r\n";
			body += "\tvolume\r\n";
			body += "\tmode\r\n";
			body += "\tstrokecount\r\n";

			mg_http_reply(conn, 200, headers.c_str(), body.c_str());
		}
		else if(mg_http_match_uri(m, "/webhook"))		// Virtual Succubus
		{
			std::vector<std::pair<std::string, std::string>> params;

			t.GetQueryVar(&m->query, "action", params);
			t.GetQueryVar(&m->query, "strokedepth", params);
			t.GetQueryVar(&m->query, "strokespeed", params);
			t.GetQueryVar(&m->query, "strokepattern", params);
			t.GetQueryVar(&m->query, "strokepatternEdge", params);
			t.GetQueryVar(&m->query, "strokepatternTease", params);

			if (t.m_webhookfunction)
			{
				//std::cout << "sending to webhook function" << std::endl;
				t.m_webhookfunction(params);
			}
			
			mg_http_reply(conn, 200, 0, "");
		}
		else if(mg_http_match_uri(m, "/control"))
		{
			std::string result("");
			std::vector<std::pair<std::string, std::string>> params;

			// mongoose doesn't seem to offer a way to get list of query vars, so just try each possible one and add it to vector if found
			t.GetQueryVar(&m->query, "action", params);
			t.GetQueryVar(&m->query, "frequency", params);
			t.GetQueryVar(&m->query, "spm", params);
			t.GetQueryVar(&m->query, "minpos", params);
			t.GetQueryVar(&m->query, "maxpos", params);
			t.GetQueryVar(&m->query, "volume", params);
			t.GetQueryVar(&m->query, "channelbalance", params);
			t.GetQueryVar(&m->query, "stroketype", params);
			t.GetQueryVar(&m->query, "mode", params);
			t.GetQueryVar(&m->query, "strokecount", params);
			t.GetQueryVar(&m->query, "ampshift", params);
			t.GetQueryVar(&m->query, "phaseshift", params);

			if (t.m_controlfunction)
			{
				//std::cout << "Control Function Param length=" << params.size() << std::endl;
				t.m_controlfunction(params, result);
			}
			
			mg_http_reply(conn, 200, 0, result.c_str());
		}
		else
		{
			mg_http_reply(conn, 404, 0, "");
		}
		
	}
}

void HTTPServer::SetControlFunction(std::function<void(std::vector<std::pair<std::string, std::string>>&, std::string &)> func)
{
	m_controlfunction = func;
}

void HTTPServer::SetWebhookFunction(std::function<void(std::vector<std::pair<std::string, std::string>>&)> func)
{
	m_webhookfunction = func;
}

bool HTTPServer::GetQueryVar(mg_str *uri, const std::string& var, std::vector<std::pair<std::string, std::string>>& params)
{
	std::vector<char> val(100, 0);
	int len = mg_http_get_var(uri, var.c_str(), &val[0], val.size());
	if (len >= 0)
	{
		params.push_back(std::pair<std::string, std::string>(var, std::string(val.begin(), val.begin() + len)));
		return true;
	}
	return false;
}
