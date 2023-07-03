#include "estimsignaloutput.h"

#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>

#include "fileio.h"

//debug
/*
#include <string>
*/

EstimSignalOutput::EstimSignalOutput() :m_running(false), m_stop(false), m_pstr(nullptr), m_samplerate(48000), m_mode(MODE_IDLE), m_targetmode(MODE_IDLE), m_switchmodetick(0), m_switchmodesamples(0), m_currentvolume(0), m_targetvolume(0)
{
	Pa_Initialize();

	for (int i = MODE_COUNTEDSTROKE1; i <= MODE_COUNTEDSTROKE2; i++)
	{
		StrokeParameters sp = m_strokeparameters[i];
		sp.flags |= FLAG_COUNTEDSTROKE;
		m_strokeparameters[i] = sp;
	}

}

EstimSignalOutput::~EstimSignalOutput()
{
	while (m_running)
	{
		Stop();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	if (m_pstr)
	{
		Pa_CloseStream(&m_pstr);
		m_pstr = nullptr;
	}
	Pa_Terminate();
}

bool EstimSignalOutput::IsRunning() const
{
	return m_running;
}

void EstimSignalOutput::Stop()
{
	m_stop = true;
}

void EstimSignalOutput::Start(const int64_t deviceidx, const int64_t samplerate)
{
	if (m_running)
	{
		return;
	}

	m_running = true;
	m_stop = false;

	PaStreamParameters outparam;

	memset(&outparam, 0, sizeof(PaStreamParameters));

	outparam.device = deviceidx;
	outparam.channelCount = 2;
	outparam.hostApiSpecificStreamInfo = NULL;
	outparam.sampleFormat = paFloat32;
	outparam.suggestedLatency = Pa_GetDeviceInfo(deviceidx)->defaultLowOutputLatency;

	int64_t sr = samplerate;
	PaError err = Pa_IsFormatSupported(NULL, &outparam, sr);
	//std::cout << "Err " << err << std::endl;
	if (err != paNoError && samplerate != 48000)
	{
		sr = 48000;
		err = Pa_IsFormatSupported(NULL, &outparam, sr);
	}
	if (err != paNoError && samplerate != 44100)
	{
		sr = 44100;
		err = Pa_IsFormatSupported(NULL, &outparam, sr);
	}
	if (err != paNoError && samplerate != 22050)
	{
		sr = 22050;
		err = Pa_IsFormatSupported(NULL, &outparam, sr);
	}
	if (err != paNoError && samplerate != 8000)
	{
		sr = 8000;
		err = Pa_IsFormatSupported(NULL, &outparam, sr);
	}

	if (err == paNoError)
	{
		//m_frequency = frequency;
		m_samplerate = sr;
		//std::cout << "Opening stream " << outparam.device << " " << m_samplerate << std::endl;
		err = Pa_OpenStream(&m_pstr, NULL, &outparam, sr, paFramesPerBufferUnspecified, paNoFlag, &EstimSignalOutput::SignalCallback, this);

		if (err == paNoError)
		{
			std::thread t = std::thread(&EstimSignalOutput::Run, this);
			t.detach();
		}
		else
		{
			std::cerr << "Error opening stream " << err << std::endl;
		}
	}
	else
	{
		m_stop = false;
		m_running = false;
	}
}

void EstimSignalOutput::Run()
{
	m_running = true;
	m_stop = false;

	//std::cout << "Starting stream" << std::endl;
	PaError err = Pa_StartStream(m_pstr);
	if (err != paNoError)
	{
		std::cerr << "Error starting stream " << err << std::endl;
	}

	do
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	} while (!m_stop);

	Pa_StopStream(m_pstr);

	//std::cout << "Stopped stream" << std::endl;

	m_stop = false;
	m_running = false;
}

void EstimSignalOutput::SetCurrentModeStrokeParameters(const int8_t stroketype, const std::vector<int64_t> frequencies, const int64_t strokerate, const int64_t minpos, const int64_t maxpos, const int64_t volume, const int32_t channelbalance, const int32_t ampshift, const int32_t phaseshift)
{
	SetStrokeParameters(m_mode, stroketype, frequencies, strokerate, minpos, maxpos, volume, channelbalance, ampshift, phaseshift);
}

void EstimSignalOutput::SetStrokeParameters(const int mode, const int8_t stroketype, const std::vector<int64_t> frequencies, const int64_t strokerate, const int64_t minpos, const int64_t maxpos, const int64_t volume, const int32_t channelbalance, const int32_t ampshift, const int32_t phaseshift)
{
	if (mode >= MODE_IDLE && mode < MODE_MAX)		// make sure mode in range, caller must ensure that idle mode values aren't changed
	{
		std::lock_guard<std::mutex> guard(m_strokemutex);
		StrokeParameters strokeparameters = m_strokeparameters[mode].load();

		strokeparameters.stroketype = ((stroketype > -1 && stroketype < STROKE_MAX) ? stroketype : strokeparameters.stroketype);
		const int64_t sr = (strokerate > -1 ? strokerate : strokeparameters.strokerate);
		const int64_t minp = std::max(0ll, (minpos > -1 ? minpos : strokeparameters.minstrokepos));
		const int64_t maxp = std::max(std::min(100ll, (maxpos > -1 ? maxpos : strokeparameters.maxstrokepos)), minp);
		const double vol = std::max(0.0, (volume > -1 ? static_cast<double>(volume) / 100.0 : strokeparameters.targetampmult));
		strokeparameters.channelbalance = ((channelbalance > -1 && channelbalance <= 200) ? channelbalance : strokeparameters.channelbalance);
		strokeparameters.ampshift = ((ampshift >= -180 && ampshift <= 180) ? ampshift : strokeparameters.ampshift);
		strokeparameters.phaseshift = ((phaseshift >= -180 && phaseshift <= 180) ? phaseshift : strokeparameters.phaseshift);

		if (frequencies.size() > 0)
		{
			for (int i = 0; i < strokeparameters.frequencydata.size(); i++)
			{
				if (frequencies.size() > i && frequencies[i] > 0)
				{
					strokeparameters.frequencydata[i].frequency = frequencies[i];
					strokeparameters.frequencydata[i].incphase = (static_cast<double>(strokeparameters.frequencydata[i].frequency) / static_cast<double>(m_samplerate)) * (2.0 * M_PI);
				}
				else
				{
					strokeparameters.frequencydata[i].frequency = 0;
					strokeparameters.frequencydata[i].incphase = 0;
					strokeparameters.frequencydata[i].currentphase = 0;
				}
			}
		}
		strokeparameters.strokerate = sr;
		strokeparameters.minstrokepos = minp;
		strokeparameters.maxstrokepos = maxp;
		strokeparameters.targetampmult = vol;

		if (strokeparameters.targetampmult < strokeparameters.currentampmult)
		{
			strokeparameters.currentampmult = strokeparameters.targetampmult;
		}

		strokeparameters.leftampmult = 1.0;
		strokeparameters.rightampmult = 1.0;
		if (strokeparameters.channelbalance < 100)
		{
			strokeparameters.rightampmult = 1.0 - (static_cast<double>(100 - channelbalance) / 100.0);
		}
		else if (strokeparameters.channelbalance > 100)
		{
			strokeparameters.leftampmult = (static_cast<double>(200 - channelbalance) / 100.0);
		}

		// stroke % per sample
		//(strokerate / 60.0 / m_samplerate);

		// 2* because we need to go back and forth for 1 complete stroke
		strokeparameters.incstrokepos = (2.0 * static_cast<double>(strokeparameters.maxstrokepos - strokeparameters.minstrokepos)) * (static_cast<double>(strokeparameters.strokerate) / 60.0 / static_cast<double>(m_samplerate));
		// check if current stroke pos is < 0 and make sure inc is +, check if current stroke pos is > 100 and make sure inc is -
		if (strokeparameters.currentstrokepos < strokeparameters.minstrokepos && strokeparameters.incstrokepos < 0)
		{
			strokeparameters.incstrokepos = -strokeparameters.incstrokepos;
		}
		if (strokeparameters.currentstrokepos > strokeparameters.maxstrokepos && strokeparameters.incstrokepos > 0)
		{
			strokeparameters.minstrokepos = -strokeparameters.incstrokepos;
		}

		m_strokeparameters[mode].store(strokeparameters);

		//std::cout << "minp=" << minp << " maxp=" << maxp << " minpos=" << strokeparameters.minstrokepos << " maxpos=" << strokeparameters.maxstrokepos << std::endl;
		//std::cout << "incpos=" << strokeparameters.incstrokepos << std::endl;
	}
}

void EstimSignalOutput::AddStrokes(const int mode, const int64_t strokes)
{
	if (mode >= 0 && mode < MODE_MAX && strokes > 0)
	{
		std::lock_guard<std::mutex> guard(m_strokemutex);
		StrokeParameters sp = m_strokeparameters[mode];
		if(sp.FlagSet(FLAG_COUNTEDSTROKE))
		{
			sp.strokesremaining += strokes;
			m_strokeparameters[mode] = sp;
		}
	}
}

const int64_t EstimSignalOutput::GetStrokesRemaining(const int mode) const
{
	if (mode >= 0 && mode < MODE_MAX)
	{
		StrokeParameters sp = m_strokeparameters[mode];
		return sp.strokesremaining;
	}
	return 0;
}

const int64_t EstimSignalOutput::GetStrokeRate(const int mode) const
{
	if (mode >= 0 && mode < MODE_MAX)
	{
		StrokeParameters sp = m_strokeparameters[mode];
		return sp.strokerate;
	}
	return 0;
}

void EstimSignalOutput::SetMasterVolume(const int64_t volume)
{
	if (volume >= 0 && volume <= 100)
	{
		std::lock_guard<std::mutex> guard(m_strokemutex);
		m_targetvolume = static_cast<double>(volume) / 100.0;
		if (m_targetvolume < m_currentvolume)	// always adjust volume down immediately
		{
			m_currentvolume = static_cast<double>(volume) / 100.0;
		}
	}
}

void EstimSignalOutput::SetMode(const int mode)
{
	if (mode >= 0 && mode < MODE_MAX && mode != m_mode)
	{
		std::lock_guard<std::mutex> guard(m_strokemutex);
		//m_currentvolume = 0;										// always set volume to 0 when first changing mode - don't set master volume, set mode current volume
		StrokeParameters p = m_strokeparameters[mode];
		p.currentampmult = 0;
		p.ResetPhase();	// reset back to 0
		p.strokesremaining = 0;										// clear any existing strokes remaining
		m_strokeparameters[mode] = p;
		//m_mode = mode;
		m_targetmode = mode;
		m_switchmodetick = 0;
		m_switchmodesamples = m_samplerate * 25ll / 1000ll;		// mode will switch in 25ms, giving time to change volume to 0
	}
}

int EstimSignalOutput::SignalCallback(const void* input, void* output, unsigned long framecount, const PaStreamCallbackTimeInfo* timeinfo, PaStreamCallbackFlags statusflags, void* userdata)
{
	EstimSignalOutput& s = *(EstimSignalOutput*)userdata;

	std::lock_guard<std::mutex> guard(s.m_strokemutex);
	StrokeParameters strokeparam = s.m_strokeparameters[s.m_mode].load();	// make a copy in case the parameters are changed in another thread
	//std::cout << s.m_mode << " " << strokeparam.currentphase << " " << s.m_currentvolume << " " << strokeparam.currentampmult << std::endl;
	float* samples = (float*)output;
	double prevleft = std::numeric_limits<double>::max();
	double prevright = std::numeric_limits<double>::max();
	for (int64_t i = 0; i < framecount; i++)
	{
		const double volmult = s.m_currentvolume;
		const double strokevolmult = strokeparam.currentampmult;
		const double leftvolmult = strokeparam.leftampmult;
		const double rightvolmult = strokeparam.rightampmult;
		double leftsample = 0;
		double rightsample = 0;
		strokeparam.GetSample(leftsample, rightsample);

		samples[0] = leftsample * volmult * strokevolmult * leftvolmult;		// * master volume * stroke volume * left channel volume
		samples[1] = rightsample * volmult * strokevolmult * rightvolmult;		// * master volume * stroke volume * right channel volume
		
		if (s.m_targetvolume > s.m_currentvolume)
		{
			//double rate = (1.0 / static_cast<double>(s.m_samplerate));	// adjust volume up over 1 second
			double rate = (10.0 / static_cast<double>(s.m_samplerate));	// adjust volume up over 100ms
			s.m_currentvolume = s.m_currentvolume + rate;
			if (s.m_currentvolume > s.m_targetvolume)
			{
				s.m_currentvolume.store(s.m_targetvolume.load());
			}
		}
		else if (s.m_targetvolume < s.m_currentvolume)
		{
			//double rate = (1.0 / static_cast<double>(s.m_samplerate));	// adjust volume down over 1 second
			double rate = (10.0 / static_cast<double>(s.m_samplerate));	// adjust volume down over 100ms
			s.m_currentvolume = s.m_currentvolume - rate;
			if (s.m_currentvolume < s.m_targetvolume)
			{
				s.m_currentvolume.store(s.m_targetvolume.load());
			}
		}
		if (strokeparam.targetampmult > strokeparam.currentampmult)
		{
			double rate = (10.0 / static_cast<double>(s.m_samplerate));	// adjust volume up over 100ms
			strokeparam.currentampmult = strokeparam.currentampmult + rate;
			if (strokeparam.currentampmult > strokeparam.targetampmult)
			{
				strokeparam.currentampmult = strokeparam.targetampmult;
			}
		}
		else if (strokeparam.targetampmult < strokeparam.currentampmult)
		{
			double rate = (10.0 / static_cast<double>(s.m_samplerate));	// adjust volume down over 100ms
			strokeparam.currentampmult = strokeparam.currentampmult - rate;
			if (strokeparam.currentampmult < strokeparam.targetampmult)
			{
				strokeparam.currentampmult = strokeparam.targetampmult;
			}
		}

		strokeparam.IncrementStrokePosition(s.m_samplerate);

		strokeparam.IncrementPhase();

		// check if we're waiting to switch modes, and do it if needed, or alter volume as needed
		if (s.m_mode != s.m_targetmode)
		{
			s.m_switchmodetick++;
			if (s.m_switchmodetick >= s.m_switchmodesamples)
			{
				s.m_switchmodetick = 0;
				s.m_switchmodesamples = 0;
				s.m_strokeparameters[s.m_mode] = strokeparam;
				s.m_mode = s.m_targetmode;
				strokeparam = s.m_strokeparameters[s.m_mode];
			}
			// waiting to switch modes - adjust volume based on how close we are to switching
			else
			{
				double volmult = 1.0 - (static_cast<double>(s.m_switchmodetick) / static_cast<double>(s.m_switchmodesamples));
				samples[0] *= volmult;
				samples[1] *= volmult;
			}
		}

		samples += 2;
		prevleft = samples[0];
		prevright = samples[0];
	}

	s.m_strokeparameters[s.m_mode] = strokeparam;

	return 0;
}

void EstimSignalOutput::GetAudioDevices(std::map<int64_t, std::string>& devices)
{
	Pa_Initialize();

	devices.clear();

	PaDeviceIndex dc = Pa_GetDeviceCount();
	for (int i = 0; i < dc; i++)
	{
		const PaDeviceInfo* dev = Pa_GetDeviceInfo(i);
		if (dev->maxOutputChannels > 0)
		{
			devices[i] = "(" + std::string(Pa_GetHostApiInfo(dev->hostApi)->name) + ") - " + std::string(dev->name);
		}
	}

	Pa_Terminate();
}

const bool EstimSignalOutput::LoadSettings(const std::string& filename)
{
	FileIO file;
	if (file.OpenRead(filename))
	{
		std::string hdr("");
		int32_t ver = -1;
		file.ReadStr(hdr, 4);
		file.ReadInt32(ver);
		if (hdr == "ECFG" && ver == 1)
		{
			int64_t vol = -1;
			file.ReadInt64(vol);
			SetMasterVolume(vol);

			for (int i = 1; i < MODE_MAX; i++)
			{
				uint8_t flags = 0;
				int8_t stroketype = -1;
				std::vector<int64_t> frequencies(10, -1);
				int64_t strokerate = -1;
				int64_t minstrokepos = -1;
				int64_t maxstrokepos = -1;
				int64_t volume = -1;
				int32_t channelbalance = -1;
				int32_t ampshift = -10000;
				int32_t phaseshift = -10000;

				if (file.ReadUInt8(flags) &&
					file.ReadInt8(stroketype) &&
					file.ReadInt64(frequencies[0]) &&
					file.ReadInt64(frequencies[1]) &&
					file.ReadInt64(frequencies[2]) &&
					file.ReadInt64(frequencies[3]) &&
					file.ReadInt64(frequencies[4]) &&
					file.ReadInt64(frequencies[5]) &&
					file.ReadInt64(frequencies[6]) &&
					file.ReadInt64(frequencies[7]) &&
					file.ReadInt64(frequencies[8]) &&
					file.ReadInt64(frequencies[9]) &&
					file.ReadInt64(strokerate) &&
					file.ReadInt64(minstrokepos) &&
					file.ReadInt64(maxstrokepos) &&
					file.ReadInt64(volume) &&
					file.ReadInt32(channelbalance) &&
					file.ReadInt32(ampshift) &&
					file.ReadInt32(phaseshift)
					)
				{

					SetStrokeParameters(i, stroketype, frequencies, strokerate, minstrokepos, maxstrokepos, volume, channelbalance, ampshift, phaseshift);
				}
				else
				{
					return false;
				}
			}
		}
		else
		{
			std::cerr << "Header or version mismatch ! " << hdr << " " << ver << std::endl;
			return false;
		}
		file.Close();
		return true;
	}
	return false;
}

const bool EstimSignalOutput::SaveSettings(const std::string& filename)
{
	FileIO file;
	if (file.OpenWrite(filename))
	{
		file.WriteStr("ECFG");
		file.WriteInt32(1);
		file.WriteInt64(m_targetvolume*100.0);
		for (int i = 1; i < MODE_MAX; i++)
		{
			StrokeParameters params = m_strokeparameters[i];
			file.WriteUInt8(params.flags);
			file.WriteInt8(params.stroketype);
			for (int i = 0; i < params.frequencydata.size(); i++)
			{
				file.WriteInt64(params.frequencydata[i].frequency);
			}
			file.WriteInt64(params.strokerate);
			file.WriteInt64(params.minstrokepos);
			file.WriteInt64(params.maxstrokepos);
			file.WriteInt64(params.targetampmult * 100.0);
			file.WriteInt32(params.channelbalance);
			file.WriteInt32(params.ampshift);
			file.WriteInt32(params.phaseshift);
		}
		file.Close();
		return true;
	}
	return false;
}

const int EstimSignalOutput::GetCurrentMode() const
{
	return m_mode;
}

const int64_t EstimSignalOutput::GetSampleRate() const
{
	return m_samplerate;
}

const int64_t EstimSignalOutput::GetCurrentVolume() const
{
	return (m_currentvolume * 100.0);
}

const int64_t EstimSignalOutput::GetTargetVolume() const
{
	return (m_targetvolume * 100.0);
}

void EstimSignalOutput::GetStrokeParameters(std::vector<StrokeParameters>& params)
{
	params.clear();
	for (int i = 0; i < MODE_MAX; i++)
	{
		params.push_back(m_strokeparameters[i].load());
	}
}

EstimSignalOutput::StrokeFrequencyPhaseData::StrokeFrequencyPhaseData() :frequency(0), currentphase(0), incphase(0)
{

}

EstimSignalOutput::StrokeParameters::StrokeParameters() :
	flags(0), stroketype(STROKE_FREQUENCY),
	strokerate(0), strokesremaining(0),
	currentstrokepos(0), maxstrokepos(0),
	targetampmult(1), currentampmult(1),
	channelbalance(100), leftampmult(1), rightampmult(1),
	ampshift(0), phaseshift(0)
{

}

const bool EstimSignalOutput::StrokeParameters::FlagSet(const int flag) const
{
	return ((flags & flag) == flag);
}

void EstimSignalOutput::StrokeParameters::GetSample(double& left, double& right) const
{
	left = 0;
	right = 0;

	// left channel won't get shifted, just right channel in relation to left channel
	const double rightphaseoffset = static_cast<double>(phaseshift) * M_PI / 180.0;

	for (int i = 0; i < frequencydata.size(); i++)
	{
		if (frequencydata[i].frequency > 0)
		{
			double leftphase = frequencydata[i].currentphase;
			double rightphase = frequencydata[i].currentphase;

			if (stroketype == STROKE_FREQUENCY)
			{
				rightphase += (static_cast<double>(currentstrokepos) / 100.0) * M_PI;
			}

			rightphase += rightphaseoffset;

			//left += sinf(frequencydata[i].currentphase);
			//right += sinf(frequencydata[i].currentphase + currentstrokerad);		// TODO - add offset
			left += sinf(leftphase);
			right += sinf(rightphase);
		}
	}

	if (stroketype == STROKE_AMPLITUDE)
	{
		// change amplitude based on stroke position

		double rightstrokepos = currentstrokepos;
		
		// check for amplitude offset and apply it to amplitude
		if (ampshift != 0)
		{
			const double shiftpos = (static_cast<double>(ampshift) / 180.0) * static_cast<double>(minstrokepos) * (incstrokepos < 0 ? -1.0 : 1.0);
			rightstrokepos += shiftpos;
			while(rightstrokepos > maxstrokepos || rightstrokepos < minstrokepos)
			{
				while (rightstrokepos > maxstrokepos)
				{
					rightstrokepos = static_cast<double>(maxstrokepos) - (rightstrokepos - static_cast<double>(maxstrokepos));
				}
				while (rightstrokepos < minstrokepos)
				{
					rightstrokepos = static_cast<double>(minstrokepos) + (static_cast<double>(minstrokepos) - rightstrokepos);
				}
			}
		}
		
		double smoothedleftstrokepos = currentstrokepos;
		double smoothedrightstrokepos = rightstrokepos;
		
		if (currentstrokepos >= minstrokepos && currentstrokepos <= maxstrokepos && rightstrokepos >= minstrokepos && rightstrokepos <= maxstrokepos)
		{
			// TODO - smooth pos with sin function
			// TODO - only do if currentpos is within min and max pos
			//sinf(((current-min)/range)*M_PI)
			const double strokerange = static_cast<double>(maxstrokepos - minstrokepos);
			const double strokehalfrange = strokerange / 2.0;
			const double strokemidpos = static_cast<double>(maxstrokepos + minstrokepos) / 2.0;

			// this gives smoothed "hills" with the valleys being sharp because we're always in the 1st or 2nd quadrant
			//const double leftsin = sinf(((currentstrokepos - static_cast<double>(minstrokepos)) / (strokerange)) * M_PI);
			//const double rightsin = sinf(((rightstrokepos - static_cast<double>(minstrokepos)) / (strokerange)) * M_PI);
			//smoothedleftstrokepos = minstrokepos + (strokerange * leftsin);
			//smoothedrightstrokepos = minstrokepos + (strokerange * rightsin);			
			
			const double leftsin = sinf(((currentstrokepos - strokemidpos) / strokerange) * M_PI);
			const double rightsin = sinf(((rightstrokepos - strokemidpos) / strokerange) * M_PI);
			smoothedleftstrokepos = strokemidpos + (strokehalfrange * leftsin);
			smoothedrightstrokepos = strokemidpos + (strokehalfrange * rightsin);
		}

		//left *= (static_cast<double>(currentstrokepos) / 100.0);
		//right *= (static_cast<double>(rightstrokepos) / 100.0);
		left *= (static_cast<double>(smoothedleftstrokepos) / 100.0);
		right *= (static_cast<double>(smoothedrightstrokepos) / 100.0);
	}

	left = std::max(-1.0, std::min(1.0, left));
	right = std::max(-1.0, std::min(1.0, right));
}

void EstimSignalOutput::StrokeParameters::ResetPhase()
{
	for (int i = 0; i < frequencydata.size(); i++)
	{
		frequencydata[i].currentphase = 0;
	}
}

void EstimSignalOutput::StrokeParameters::IncrementPhase()
{
	for (int i = 0; i < frequencydata.size(); i++)
	{
		frequencydata[i].currentphase += frequencydata[i].incphase;
		while (frequencydata[i].currentphase >= (2.0 * M_PI))
		{
			frequencydata[i].currentphase -= (2.0 * M_PI);
		}
	}
}

void EstimSignalOutput::StrokeParameters::IncrementStrokePosition(const int64_t samplerate)
{
	if (minstrokepos != maxstrokepos)	// stroking between min and max
	{
		// only increment the stroke position if we're not in counted mode, or strokes remaining > 0
		if (strokesremaining > 0 || FlagSet(FLAG_COUNTEDSTROKE) == false)
		{
			currentstrokepos += incstrokepos;
		}
		if (incstrokepos > 0 && currentstrokepos > static_cast<double>(maxstrokepos))
		{
			currentstrokepos = static_cast<double>(maxstrokepos) - (currentstrokepos - static_cast<double>(maxstrokepos));
			incstrokepos = -incstrokepos;
		}
		if (incstrokepos < 0 && currentstrokepos < static_cast<double>(minstrokepos))
		{
			currentstrokepos = static_cast<double>(minstrokepos) + (static_cast<double>(minstrokepos) - currentstrokepos);
			incstrokepos = -incstrokepos;
			// we've reached the bottom of the stroke, so remove one from remaining stroke count (if set)
			if (strokesremaining > 0)
			{
				strokesremaining--;
			}
			// if we're in counted mode at the bottom of a stroke and no more left, make sure position stays at bottom
			if (strokesremaining == 0 && FlagSet(FLAG_COUNTEDSTROKE))
			{
				currentstrokepos = 0.0;
			}
		}
	}
	else if (currentstrokepos != static_cast<double>(maxstrokepos))	// no stroking - just one position and we're not yet at that position - move to it
	{
		double rate = M_PI / static_cast<double>(samplerate);		// pos 0-100 in 1 second
		if (strokerate > 0)	// if we have a stroke rate, then use that to compute the rate
		{
			rate = M_PI * static_cast<double>(strokerate) / 60.0 / static_cast<double>(samplerate);
		}
		if (currentstrokepos > static_cast<double>(maxstrokepos))	// reduce until we get to the expected value
		{
			currentstrokepos -= rate;
			if (currentstrokepos < static_cast<double>(maxstrokepos))
			{
				currentstrokepos = maxstrokepos;
			}
		}
		else if (currentstrokepos < static_cast<double>(minstrokepos))	// increase until we get to the expected value
		{
			currentstrokepos += rate;
			if (currentstrokepos > static_cast<double>(minstrokepos))
			{
				currentstrokepos = static_cast<double>(minstrokepos);
			}
		}
	}
}
