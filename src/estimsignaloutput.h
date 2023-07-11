#pragma once

#include <atomic>
#include <mutex>
#include <array>
#include <map>
#include <vector>
#include <portaudio.h>

class EstimSignalOutput
{
public:
	EstimSignalOutput();
	~EstimSignalOutput();

	bool IsRunning() const;
	void Stop();
	void Start(const int64_t deviceidx, const int64_t samplerate/*, const int64_t frequency*/);

	const bool LoadSettings(const std::string& filename);
	const bool SaveSettings(const std::string& filename);

	void SetCurrentModeStrokeParameters(const int8_t stroketype, const std::vector<int64_t> frequencies, const int64_t strokerate, const int64_t minpos, const int64_t maxpos, const int64_t volume, const int32_t balance, const int32_t ampshift, const int32_t phaseshift);
	void SetStrokeParameters(const int mode, const int8_t stroketype, const std::vector<int64_t> frequencies, const int64_t strokerate, const int64_t minpos, const int64_t maxpos, const int64_t volume, const int32_t balance, const int32_t ampshift, const int32_t phaseshift);
	void SetEventStrokeParameters(const int mode, const int64_t strokerate, const int64_t eventminpos, const int64_t eventmaxpos);
	void AddStrokes(const int mode, const int64_t strokes);
	void SetMasterVolume(const int64_t volume);
	void SetMode(const int mode);

	enum Flag
	{
		FLAG_COUNTEDSTROKE = 0b00000001,
		FLAG_PATTERNSTROKE = 0b00000010
	};

	enum StrokeType
	{
		STROKE_FREQUENCY = 0,
		STROKE_AMPLITUDE = 1,
		STROKE_MAX
	};

	enum Mode
	{
		MODE_IDLE = 0,
		MODE_COUNTEDSTROKE1,
		MODE_COUNTEDSTROKE2,
		MODE_STROKE1,
		MODE_STROKE2,
		MODE_TEASING1,
		MODE_TEASING2,
		MODE_EDGE1,
		MODE_EDGE2,
		MODE_PAIN1,
		MODE_PAIN2,
		MODE_USER1,
		MODE_USER2,
		MODE_USER3,
		MODE_USER4,
		MODE_USER5,
		MODE_USER6,
		MODE_USER7,
		MODE_USER8,
		MODE_USER9,
		MODE_USER10,
		MODE_MAX
	};

	// waveform not used yet
	enum Waveform
	{
		WAVEFORM_SINE=0,
		WAVEFORM_SAWTOOTH=1,
		WAVEFORM_TRIANGLE=2,
		WAVEFORM_SQUARE=3,
		WAVEFORM_NOISE=4,
		WAVEFORM_MAX
	};

	struct StrokeFrequencyPhaseData
	{
		StrokeFrequencyPhaseData();
		~StrokeFrequencyPhaseData() = default;

		int64_t frequency;		// carrier frequency (for info purposes only)
		double currentphase;	// current phase in radians
		double incphase;		// increment phase per sample;
	};

	struct StrokeParameters
	{
		StrokeParameters();
		~StrokeParameters() = default;	// default - otherwise it's not trivially copyable, and can't be an atomic

		const bool FlagSet(const int flag) const;

		void GetSample(double& left, double& right) const;

		void RecalculateStrokeIncrement(const int64_t samplerate);		// recalculates and sets incstrokepos based on current parameters

		void ResetPhase();
		void IncrementPhase();
		void IncrementStrokePosition(const int64_t samplerate);

		int64_t CalculatedMinStrokePos() const;
		int64_t CalculatedMaxStrokePos() const;

		uint8_t flags;
		int8_t stroketype;

		std::array<StrokeFrequencyPhaseData, 10> frequencydata;

		int64_t strokerate;			// strokes per minute
		int64_t strokesremaining;	// in counted mode, how many strokes are left

		int64_t modeminstrokepos;		// 0 - 100
		int64_t modemaxstrokepos;		// 0 - 100

		int64_t eventminstrokepos;		// for VS stroke event only (not saved) - used for AM stroke type
		int64_t eventmaxstrokepos;		// for VS stroke event only (not saved) - used for AM stroke type

		double currentstrokepos;	// 0 - 100
		double incstrokepos;		// + or - value to add to current pos each sample (then reversed when passes min or max)

		double targetampmult;	// target amplitude multiplier
		double currentampmult;	// current amplitude multiplier

		int32_t channelbalance;
		double leftampmult;		// left channel amplitude multiplier
		double rightampmult;	// right channel amplitude multiplier

		int32_t ampshift;		// -180 to 180 degrees
		int32_t phaseshift;		// -180 to 180 degrees
	};

	const int GetCurrentMode() const;
	const int64_t GetSampleRate() const;
	const int64_t GetCurrentVolume() const;
	const int64_t GetTargetVolume() const;
	const int64_t GetStrokesRemaining(const int mode) const;
	const int64_t GetStrokeRate(const int mode) const;
	void GetStrokeParameters(std::vector<StrokeParameters>& params);

	static void GetAudioDevices(std::map<int64_t, std::string>& devices);

private:

	std::atomic<bool> m_running;
	std::atomic<bool> m_stop;

	PaStream* m_pstr;
	int64_t m_samplerate;
	int m_mode;
	int m_targetmode;

	int64_t m_switchmodetick;
	int64_t m_switchmodesamples;

	std::mutex m_strokemutex;
	std::array<std::atomic<StrokeParameters>, MODE_MAX> m_strokeparameters;

	std::atomic<double> m_currentvolume;	// master volume - will change every sample to get to targetvolume
	std::atomic<double> m_targetvolume;		// master volume

	void Run();
	static int SignalCallback(const void* input, void* output, unsigned long framecount, const PaStreamCallbackTimeInfo* timeinfo, PaStreamCallbackFlags statusflags, void* userdata);

};