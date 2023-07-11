#include <iostream>
#include <atomic>
#include <thread>
#include <sstream>
#include <map>
#include <chrono>
#include <portaudio.h>

#include "httpserver.h"
#include "estimsignaloutput.h"
#include "miscfunctions.h"

struct programoptions
{
	int64_t deviceidx;
	std::string listenaddress;
	std::string listenport;
};

std::atomic<bool> running = false;

void sighandler(int signo)
{
	running = false;
}

bool getvar(const std::vector<std::pair<std::string, std::string>>& params, const std::string& var, std::string& val)
{
	for (std::vector<std::pair<std::string, std::string>>::const_iterator i = params.begin(); i != params.end(); i++)
	{
		if ((*i).first == var)
		{
			val = (*i).second;
			return true;
		}
	}
	return false;
}

bool getvar(const std::vector<std::pair<std::string, std::string>>& params, const std::string& var, int32_t& val)
{
	std::string v;
	if (getvar(params, var, v))
	{
		std::istringstream istr(v);
		istr >> val;
		return true;
	}
	return false;
}

bool getvar(const std::vector<std::pair<std::string, std::string>>& params, const std::string& var, int64_t& val)
{
	std::string v;
	if (getvar(params, var, v))
	{
		std::istringstream istr(v);
		istr >> val;
		return true;
	}
	return false;
}

void listaudiodevices()
{
	std::map<int64_t, std::string> devices;
	EstimSignalOutput::GetAudioDevices(devices);

	for (std::map<int64_t, std::string>::const_iterator i = devices.begin(); i != devices.end(); i++)
	{
		std::cout << (*i).first << "\t" << (*i).second << std::endl;
	}
}

void printoptions()
{
	std::cout << "Command Line Options:" << std::endl;
	std::cout << "-d [devid]             Use device id as audio output" << std::endl;
	std::cout << "-h [ip | hostname]     Bind HTTP service to ip or hostname" << std::endl;
	std::cout << "-p [port]              Bind HTTP service to port" << std::endl;
	std::cout << "-list                  List available audio output devices" << std::endl;
}

int main(int argc, char* argv[])
{
	signal(SIGINT, sighandler);
	signal(SIGABRT, sighandler);
	signal(SIGBREAK, sighandler);

	programoptions opts;

	opts.deviceidx = 0;
	opts.listenaddress = "0.0.0.0";
	opts.listenport = "8082";

	running = true;

	for (int i = 1; i < argc;)
	{
		std::string arg("");
		if (argv[i])
		{
			arg = std::string(argv[i]);
		}
		if (arg == "-d" && (i + 1) < argc && argv[i + 1])
		{
			std::string str(argv[i + 1]);
			std::istringstream istr(str);
			istr >> opts.deviceidx;
			i += 2;
		}
		else if (arg == "-h" && (i + 1) < argc && argv[i + 1])
		{
			opts.listenaddress = std::string(argv[i + 1]);
			i += 2;
		}
		else if (arg == "-p" && (i + 1) < argc && argv[i + 1])
		{
			opts.listenport = std::string(argv[i + 1]);
			i += 2;
		}
		else if (arg == "-list")
		{
			listaudiodevices();
			return 0;
		}
		else if (arg == "-?" || arg == "-help")
		{
			printoptions();
			return 0;
		}
		else
		{
			std::cerr << "Unexpected argument " << arg << std::endl;
			printoptions();
			i++;
			return -1;
		}
	}

	HTTPServer http;
	http.Start(opts.listenaddress, opts.listenport);

	std::cout << "Listening at http://" << opts.listenaddress << ":" << opts.listenport << std::endl;

	EstimSignalOutput sig;
	sig.Start(opts.deviceidx, 48000);
	std::vector<int64_t> freq(1, 0);
	sig.SetStrokeParameters(0, 0, freq, 0, 0, 0, 0, 100, 0, 0);

	sig.LoadSettings("settings");

	int edgetype = 1;
	std::chrono::system_clock::time_point lastrhythmtick = std::chrono::system_clock::now();

	http.SetControlFunction([&sig](std::vector<std::pair<std::string, std::string>>& params, std::string& result)->void
		{
			result = "";
			std::string action("");
			getvar(params, "action", action);

			//std::cout << "Got control message! action=" << action << " paramlen=" << params.size() << std::endl;

			if (action == "stop")
			{
				sig.SetMode(EstimSignalOutput::MODE_IDLE);
			}
			else if (action == "addstrokes")
			{
				int64_t mode = -1;
				int64_t strokecount = -1;
				getvar(params, "mode", mode);
				getvar(params, "strokecount", strokecount);

				sig.AddStrokes(mode, strokecount);
			}
			else if (action == "strokeconfig")
			{
				int64_t spm = -1;
				int64_t minpos = -1;
				int64_t maxpos = -1;
				int64_t vol = -1;
				int32_t channelbalance = -1;
				int64_t mode = -1;
				int64_t stroketype = -1;
				int32_t ampshift = -10000;
				int32_t phaseshift = -10000;
				std::string freqstr = "";
				getvar(params, "stroketype", stroketype);
				getvar(params, "frequency", freqstr);
				getvar(params, "spm", spm);
				getvar(params, "minpos", minpos);
				getvar(params, "maxpos", maxpos);
				getvar(params, "volume", vol);
				getvar(params, "channelbalance", channelbalance);
				getvar(params, "ampshift", ampshift);
				getvar(params, "phaseshift", phaseshift);
				getvar(params, "mode", mode);

				std::vector<int64_t> freqs;
				if (freqstr != "-1")
				{
					SplitString(freqstr, ",", freqs);
				}

				//std::cout << "Setting stroke parameters for mode " << mode << " type=" << stroketype << " spm=" << spm << " minpos=" << minpos << " maxpos=" << maxpos << " vol=" << vol << " cb=" << channelbalance << " as=" << ampshift << " ps=" << phaseshift << std::endl;
				if (mode != EstimSignalOutput::MODE_IDLE)
				{
					sig.SetStrokeParameters(mode, stroketype, freqs, spm, minpos, maxpos, vol, channelbalance, ampshift, phaseshift);
				}
			}
			else if (action == "setvolume")
			{
				int64_t vol = -1;
				getvar(params, "volume", vol);
				sig.SetMasterVolume(vol);
			}
			else if (action == "setmode")
			{
				int64_t mode = -1;
				getvar(params, "mode", mode);
				sig.SetMode(mode);
			}
			else if (action == "getconfig")
			{
				std::vector<EstimSignalOutput::StrokeParameters> params;
				sig.GetStrokeParameters(params);

				result = "{";
				result += "\"currentmode\":" + std::to_string(sig.GetCurrentMode());
				result += ",\"currentvolume\":" + std::to_string(sig.GetCurrentVolume());
				result += ",\"targetvolume\":" + std::to_string(sig.GetTargetVolume());
				result += ",\"modes\":[";
				for (int i = 0; i < params.size(); i++)
				{
					bool needcomma = false;
					if (i > 0)
					{
						result += ",";
					}
					result += "{";
					result += "\"mode\":" + std::to_string(i);
					result += ",\"stroketype\":" + std::to_string(params[i].stroketype);
					result += ",\"frequency\":[";
					for (int j = 0; j < params[i].frequencydata.size(); j++)
					{
						if (params[i].frequencydata[j].frequency > 0)
						{
							if (needcomma == true)
							{
								result += ",";
							}
							result += std::to_string(params[i].frequencydata[j].frequency);
							needcomma = true;
						}
					}
					result += "]";
					result += ",\"spm\":" + std::to_string(params[i].strokerate);
					result += ",\"minpos\":" + std::to_string(params[i].modeminstrokepos);
					result += ",\"maxpos\":" + std::to_string(params[i].modemaxstrokepos);
					result += ",\"currentvolume\":" + std::to_string(static_cast<int64_t>(params[i].currentampmult * 100.0));
					result += ",\"targetvolume\":" + std::to_string(static_cast<int64_t>(params[i].targetampmult * 100.0));
					result += ",\"channelbalance\":" + std::to_string(params[i].channelbalance);
					result += ",\"ampshift\":" + std::to_string(params[i].ampshift);
					result += ",\"phaseshift\":" + std::to_string(params[i].phaseshift);
					result += "}";
				}
				result += "]";
				result += "}";
			}
			else if (action == "saveconfig")
			{
				sig.SaveSettings("settings");
			}

		});

		http.SetWebhookFunction([&sig, &edgetype, &lastrhythmtick](std::vector<std::pair<std::string, std::string>>& params)->void
			{

				//action=Calor - TimeToDecay=1
				//action=Cumming - same time as EdgeRide and EdgeRideTE
				std::string action("");
				getvar(params, "action", action);

				if (action == "stop")
				{
					sig.SetMode(EstimSignalOutput::MODE_IDLE);
				}
				else if (action == "Teasing" || action == "TeasingTE")
				{
					sig.SetMode(EstimSignalOutput::MODE_TEASING1);
				}
				else if (action == "Punish" || action == "PunishTE")
				{
					sig.SetMode(EstimSignalOutput::MODE_PAIN1);
				}
				else if (action == "Edge" || action == "EdgeTE")
				{
					sig.SetMode(EstimSignalOutput::MODE_EDGE1);
					edgetype = 1;
				}
				else if (action == "Cumming" || action == "EdgeRide" || action == "EdgeRideTE")
				{
					sig.SetMode(EstimSignalOutput::MODE_EDGE2);
					edgetype = 2;
				}
				else if (action == "RhythmTick")
				{
					sig.SetMode(EstimSignalOutput::MODE_COUNTEDSTROKE1);
					sig.AddStrokes(EstimSignalOutput::MODE_COUNTEDSTROKE1, 1);

					// dynamically adjust the rate so we only have 1 stroke in the queue
					const int64_t rem = sig.GetStrokesRemaining(EstimSignalOutput::MODE_COUNTEDSTROKE1);
					if (rem > 1)
					{
						if (sig.GetStrokeRate(EstimSignalOutput::MODE_COUNTEDSTROKE1) < 360)
						{
							int64_t adj = (rem == 2 ? 10 : 30);
							//sig.SetStrokeParameters(EstimSignalOutput::MODE_COUNTEDSTROKE1, -1, std::vector<int64_t>(), sig.GetStrokeRate(EstimSignalOutput::MODE_COUNTEDSTROKE1) + adj, -1, -1, -1, -1, -10000, -10000);
							sig.SetEventStrokeParameters(EstimSignalOutput::MODE_COUNTEDSTROKE1, sig.GetStrokeRate(EstimSignalOutput::MODE_COUNTEDSTROKE1) + adj, -1, -1);
						}
					}
					else if (sig.GetStrokesRemaining(EstimSignalOutput::MODE_COUNTEDSTROKE1) < 2)
					{
						int64_t rate = sig.GetStrokeRate(EstimSignalOutput::MODE_COUNTEDSTROKE1);
						if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - lastrhythmtick).count() > 1.0)
						{
							rate = 60;
						}
						else
						{
							rate -= 10;
						}
						//sig.SetStrokeParameters(EstimSignalOutput::MODE_COUNTEDSTROKE1, -1, std::vector<int64_t>(), rate, -1, -1, -1, -1, -10000, -10000);
						sig.SetEventStrokeParameters(EstimSignalOutput::MODE_COUNTEDSTROKE1, rate, -1, -1);
					}

					lastrhythmtick = std::chrono::system_clock::now();
				}
				else if (action == "RhythmFinish")	// as fast as possible
				{
					sig.SetEventStrokeParameters(EstimSignalOutput::MODE_COUNTEDSTROKE1, 360, -1, -1);
					sig.SetMode(EstimSignalOutput::MODE_COUNTEDSTROKE1);
					sig.AddStrokes(EstimSignalOutput::MODE_COUNTEDSTROKE1, 100000);
				}
				else if (action == "StrokerThruster")
				{

					int64_t strokedepth = -1;
					int64_t strokespeed = -1;
					bool pattern = false;
					bool patternedge = false;
					bool patterntease = false;
					bool patternride = false;
					std::string temp = "";

					getvar(params, "strokedepth", strokedepth);
					getvar(params, "strokespeed", strokespeed);
					pattern = getvar(params, "strokepattern", temp);
					patternedge = getvar(params, "strokepatternEdge", temp);
					patternride = getvar(params, "strokepatternEdgeRide", temp);
					patterntease = getvar(params, "strokepatternTease", temp);

					// pos from Virtual Succubus XToys layout for Stroker/Thruster
					int64_t eventminpos = 0;
					int64_t eventmaxpos = 100;
					switch (strokedepth)
					{
					case 1:
						eventminpos = 80;
						eventmaxpos = 100;
						break;
					case 2:
						eventminpos = 60;
						eventmaxpos = 100;
						break;
					case 3:
						eventminpos = 40;
						eventmaxpos = 70;
						break;
					case 4:
						eventminpos = 0;
						eventmaxpos = 70;
						break;
					case 5:
						eventminpos = 0;
						eventmaxpos = 40;
						break;
					}

					if (patternedge == true)
					{
						int mode = EstimSignalOutput::MODE_EDGE1;
						switch (edgetype)
						{
						case 1:
							mode = EstimSignalOutput::MODE_EDGE1;
							break;
						case 2:
							mode = EstimSignalOutput::MODE_EDGE2;
							break;
						}
						std::vector<int64_t> freq;	// empy vector - no change to frequencies
						//sig.SetStrokeParameters(mode, -1, freq, strokespeed, minpos, maxpos, -1, -1, -10000, -10000);
						sig.SetEventStrokeParameters(mode, strokespeed, eventminpos, eventmaxpos);
						sig.SetMode(mode);
					}
					else if (patterntease == true)
					{
						std::vector<int64_t> freq;	// empy vector - no change to frequencies
						//sig.SetStrokeParameters(EstimSignalOutput::MODE_TEASING1, -1, freq, strokespeed, minpos, maxpos, -1, -1, -10000, -10000);
						sig.SetEventStrokeParameters(EstimSignalOutput::MODE_TEASING1, strokespeed, eventminpos, eventmaxpos);
						sig.SetMode(EstimSignalOutput::MODE_TEASING1);
					}
					else if (patternride == true)
					{
						sig.SetMode(EstimSignalOutput::MODE_EDGE2);
					}
					else
					{
						std::vector<int64_t> freq;	// empy vector - no change to frequencies
						//sig.SetStrokeParameters(EstimSignalOutput::MODE_STROKE1, -1, freq, strokespeed, minpos, maxpos, -1, -1, -10000, -10000);
						sig.SetEventStrokeParameters(EstimSignalOutput::MODE_STROKE1, strokespeed, eventminpos, eventmaxpos);
						sig.SetMode(EstimSignalOutput::MODE_STROKE1);
					}
				}

			});

	do
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	} while (running);

	std::cout << "Stopping" << std::endl;

	http.Stop();
	sig.Stop();

	while(http.IsRunning())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	};
	while (sig.IsRunning())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	return 0;
}
