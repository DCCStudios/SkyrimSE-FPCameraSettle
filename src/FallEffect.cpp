#include "FallEffect.h"

#include <Windows.h>
#include <mmsystem.h>
#include <atomic>

#pragma comment(lib, "winmm.lib")

namespace FallEffect
{
	namespace
	{
		constexpr float PI = 3.14159265358979323846f;
		constexpr float DEG_TO_RAD = PI / 180.0f;

		// Default wav file names (relative to Data/SKSE/Plugins/FPCameraSettle/)
		constexpr const wchar_t* WIND_FILE   = L"fallingwindloop.wav";
		constexpr const wchar_t* WHINE_FILE  = L"fallingwhineloop.wav";
		constexpr const wchar_t* IMPACT_FILE = L"fallingdeathimpact.wav";

		float Clamp01(float v)
		{
			if (v < 0.0f) return 0.0f;
			if (v > 1.0f) return 1.0f;
			return v;
		}

		// Smoothstep ease (3t^2 - 2t^3)
		float SmoothStep(float t)
		{
			t = Clamp01(t);
			return t * t * (3.0f - 2.0f * t);
		}

		// Selectable fade-in/out curve shapes.
		// 0=Linear, 1=Smooth(S-curve), 2=EaseIn(slow start), 3=EaseOut(fast start), 4=Exponential(very slow start)
		float ApplyFadeCurve(float t, int curve)
		{
			t = Clamp01(t);
			switch (curve) {
				case 0: return t;
				case 1: return t * t * (3.0f - 2.0f * t);
				case 2: return t * t;
				case 3: return 1.0f - (1.0f - t) * (1.0f - t);
				case 4: return t * t * t;
				default: return t;
			}
		}

		// Skyrim per-second falling acceleration is high; convert raw velocity
		// (units/sec) to a normalized 0..1 intensity envelope based on the
		// configured trigger threshold.
		float VelocityIntensity(float a_absVelZ, float a_threshold)
		{
			if (a_threshold <= 1.0f) return 1.0f;
			// Linear ramp from threshold..(threshold * 3)
			float t = (a_absVelZ - a_threshold) / (a_threshold * 2.0f);
			return Clamp01(t);
		}
	}

	// -----------------------------------------------------------------------
	// Path helpers
	// -----------------------------------------------------------------------
	std::wstring ResolveSoundPath(const wchar_t* a_fileName)
	{
		// Place loose WAVs in Data/SKSE/Plugins/FPCameraSettle/<file>.wav.
		// Resolve absolute via the game executable directory.
		wchar_t exeBuf[MAX_PATH] = { 0 };
		DWORD   len = ::GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
		std::wstring base;
		if (len > 0 && len < MAX_PATH) {
			std::wstring full(exeBuf);
			auto pos = full.find_last_of(L"\\/");
			if (pos != std::wstring::npos) {
				base = full.substr(0, pos);
			}
		}
		std::wstring path = base;
		if (!path.empty() && path.back() != L'\\' && path.back() != L'/') {
			path += L"\\";
		}
		path += L"Data\\SKSE\\Plugins\\FPCameraSettle\\";
		path += a_fileName;
		return path;
	}

	// -----------------------------------------------------------------------
	// Audio: waveOut API
	//
	// Reads WAV files into memory ourselves (works with MO2/USVFS) and plays
	// via waveOutOpen / waveOutWrite. Volume is controlled per-handle via
	// waveOutSetVolume. For >100% volume, PCM samples are pre-amplified in
	// memory with hard clipping.
	// -----------------------------------------------------------------------
	namespace
	{
		// Callback-driven double-buffered streaming with per-sample software
		// volume. Eliminates waveOutSetVolume (which can bleed into game
		// audio) and hardware looping (which can glitch at buffer boundaries).
		constexpr DWORD CHUNK_MS = 50;   // ~50ms per output buffer

		struct WaveChannel;
		static void FillBuffer(WaveChannel& ch, int bufIdx);

		struct WaveChannel
		{
			HWAVEOUT                      device = nullptr;
			WAVEHDR                       headers[2] = {};
			WAVEFORMATEX                  fmt = {};
			std::vector<std::uint8_t>     sourcePcm;       // original PCM from disk
			std::vector<std::uint8_t>     chunkBuf[2];     // two small output buffers
			DWORD                         chunkBytes = 0;  // bytes per chunk (aligned)
			std::size_t                   readPos = 0;     // loop cursor in sourcePcm
			std::atomic<float>            volume{ 0.0f };  // target volume (0..5)
			std::atomic<bool>             stopping{ false };
			std::atomic<bool>             looping{ true }; // false = one-shot (no re-submit)
			bool                          loaded  = false;
			bool                          opened  = false;
			bool                          playing = false;
		};

		WaveChannel g_wind;
		WaveChannel g_whine;
		WaveChannel g_impact;

		static void CALLBACK WaveOutProc(
			HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
			DWORD_PTR dwParam1, DWORD_PTR /*dwParam2*/)
		{
			if (uMsg != WOM_DONE) return;

			auto* ch = reinterpret_cast<WaveChannel*>(dwInstance);
			if (ch->stopping.load(std::memory_order_relaxed)) return;
			if (!ch->looping.load(std::memory_order_relaxed)) return;

			auto* hdr = reinterpret_cast<WAVEHDR*>(dwParam1);
			int bufIdx = (hdr == &ch->headers[0]) ? 0 : 1;
			FillBuffer(*ch, bufIdx);
			::waveOutWrite(hwo, hdr, sizeof(WAVEHDR));
		}

		// Copy the next chunk from sourcePcm (wrapping for seamless loop)
		// and apply software volume scaling to the chunk.
		static void FillBuffer(WaveChannel& ch, int bufIdx)
		{
			auto* dst = ch.chunkBuf[bufIdx].data();
			std::size_t remaining = ch.chunkBytes;
			std::size_t srcSize = ch.sourcePcm.size();
			bool loop = ch.looping.load(std::memory_order_relaxed);

			while (remaining > 0) {
				std::size_t avail = srcSize - ch.readPos;
				if (avail == 0) {
					if (loop) {
						ch.readPos = 0;
						avail = srcSize;
					} else {
						std::memset(dst, 0, remaining);
						break;
					}
				}
				std::size_t n = (remaining < avail) ? remaining : avail;
				std::memcpy(dst, ch.sourcePcm.data() + ch.readPos, n);
				dst += n;
				remaining -= n;
				ch.readPos += n;
			}

			float vol = ch.volume.load(std::memory_order_relaxed);
			if (ch.fmt.wBitsPerSample == 16) {
				auto* samples = reinterpret_cast<std::int16_t*>(ch.chunkBuf[bufIdx].data());
				std::size_t count = ch.chunkBytes / 2;
				for (std::size_t i = 0; i < count; ++i) {
					float v = static_cast<float>(samples[i]) * vol;
					if (v >  32767.0f) v =  32767.0f;
					if (v < -32768.0f) v = -32768.0f;
					samples[i] = static_cast<std::int16_t>(v);
				}
			}
		}

		bool ReadWavFile(const std::wstring& a_path, WAVEFORMATEX& a_outFmt,
			std::vector<std::uint8_t>& a_outPcm)
		{
			HANDLE hFile = ::CreateFileW(a_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
				nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (hFile == INVALID_HANDLE_VALUE) {
				logger::warn("[FPCameraSettle][Fall][Audio] CreateFile failed: {}",
					std::filesystem::path(a_path).string());
				return false;
			}
			LARGE_INTEGER size{};
			if (!::GetFileSizeEx(hFile, &size) || size.QuadPart < 44 || size.QuadPart > (256LL * 1024 * 1024)) {
				::CloseHandle(hFile);
				logger::warn("[FPCameraSettle][Fall][Audio] WAV size out of range");
				return false;
			}
			std::vector<std::uint8_t> raw(static_cast<std::size_t>(size.QuadPart));
			DWORD bytesRead = 0;
			if (!::ReadFile(hFile, raw.data(), static_cast<DWORD>(raw.size()), &bytesRead, nullptr)
				|| bytesRead != raw.size()) {
				::CloseHandle(hFile);
				logger::warn("[FPCameraSettle][Fall][Audio] ReadFile failed");
				return false;
			}
			::CloseHandle(hFile);

			if (std::memcmp(raw.data(), "RIFF", 4) != 0 ||
				std::memcmp(raw.data() + 8, "WAVE", 4) != 0) {
				logger::warn("[FPCameraSettle][Fall][Audio] Not a RIFF/WAVE file");
				return false;
			}

			std::size_t pos = 12;
			std::size_t fmtOff = 0, fmtSize = 0;
			std::size_t dataOff = 0;
			std::uint32_t dataSize = 0;
			while (pos + 8 <= raw.size()) {
				std::uint32_t csz = raw[pos + 4] | (raw[pos + 5] << 8) |
					(raw[pos + 6] << 16) | (raw[pos + 7] << 24);
				if (std::memcmp(raw.data() + pos, "fmt ", 4) == 0) {
					fmtOff = pos + 8; fmtSize = csz;
				} else if (std::memcmp(raw.data() + pos, "data", 4) == 0) {
					dataOff = pos + 8; dataSize = csz;
					break;
				}
				pos += 8 + csz + (csz & 1);
			}
			if (!fmtOff || !dataOff || fmtSize < 16 || dataOff + dataSize > raw.size()) {
				logger::warn("[FPCameraSettle][Fall][Audio] WAV missing fmt/data chunks");
				return false;
			}

			std::memset(&a_outFmt, 0, sizeof(WAVEFORMATEX));
			a_outFmt.wFormatTag      = *reinterpret_cast<std::uint16_t*>(&raw[fmtOff]);
			a_outFmt.nChannels       = *reinterpret_cast<std::uint16_t*>(&raw[fmtOff + 2]);
			a_outFmt.nSamplesPerSec  = *reinterpret_cast<std::uint32_t*>(&raw[fmtOff + 4]);
			a_outFmt.nAvgBytesPerSec = *reinterpret_cast<std::uint32_t*>(&raw[fmtOff + 8]);
			a_outFmt.nBlockAlign     = *reinterpret_cast<std::uint16_t*>(&raw[fmtOff + 12]);
			a_outFmt.wBitsPerSample  = *reinterpret_cast<std::uint16_t*>(&raw[fmtOff + 14]);
			a_outFmt.cbSize          = 0;

			if (a_outFmt.wFormatTag != WAVE_FORMAT_PCM) {
				logger::warn("[FPCameraSettle][Fall][Audio] WAV is not PCM format (tag={})", a_outFmt.wFormatTag);
				return false;
			}

			a_outPcm.assign(raw.data() + dataOff, raw.data() + dataOff + dataSize);

			logger::info("[FPCameraSettle][Fall][Audio] Loaded WAV: {} ch, {}Hz, {}bit, {} bytes PCM from {}",
				a_outFmt.nChannels, a_outFmt.nSamplesPerSec, a_outFmt.wBitsPerSample,
				dataSize, std::filesystem::path(a_path).string());
			return true;
		}

		bool LoadChannel(WaveChannel& ch, const wchar_t* a_file)
		{
			if (ch.loaded) return true;
			std::wstring path = ResolveSoundPath(a_file);
			if (!ReadWavFile(path, ch.fmt, ch.sourcePcm)) {
				return false;
			}
			DWORD bytesPerSec = ch.fmt.nAvgBytesPerSec;
			if (bytesPerSec == 0) bytesPerSec = ch.fmt.nSamplesPerSec * ch.fmt.nChannels * (ch.fmt.wBitsPerSample / 8);
			ch.chunkBytes = (bytesPerSec * CHUNK_MS / 1000);
			if (ch.fmt.nBlockAlign > 0)
				ch.chunkBytes -= ch.chunkBytes % ch.fmt.nBlockAlign;
			if (ch.chunkBytes < 1024) ch.chunkBytes = 1024;
			ch.chunkBuf[0].resize(ch.chunkBytes, 0);
			ch.chunkBuf[1].resize(ch.chunkBytes, 0);
			ch.loaded = true;
			return true;
		}

		bool OpenChannel(WaveChannel& ch)
		{
			if (ch.opened) return true;
			MMRESULT r = ::waveOutOpen(
				&ch.device, WAVE_MAPPER, &ch.fmt,
				reinterpret_cast<DWORD_PTR>(WaveOutProc),
				reinterpret_cast<DWORD_PTR>(&ch),
				CALLBACK_FUNCTION);
			if (r != MMSYSERR_NOERROR) {
				logger::warn("[FPCameraSettle][Fall][Audio] waveOutOpen failed: MMRESULT={}", r);
				return false;
			}
			ch.opened = true;
			return true;
		}

		void PlayChannel(WaveChannel& ch)
		{
			if (!ch.opened || ch.playing || ch.sourcePcm.empty()) return;

			ch.readPos = 0;
			ch.stopping.store(false, std::memory_order_relaxed);

			for (int i = 0; i < 2; ++i) {
				FillBuffer(ch, i);
				std::memset(&ch.headers[i], 0, sizeof(WAVEHDR));
				ch.headers[i].lpData = reinterpret_cast<LPSTR>(ch.chunkBuf[i].data());
				ch.headers[i].dwBufferLength = ch.chunkBytes;
				MMRESULT r = ::waveOutPrepareHeader(ch.device, &ch.headers[i], sizeof(WAVEHDR));
				if (r != MMSYSERR_NOERROR) {
					logger::warn("[FPCameraSettle][Fall][Audio] waveOutPrepareHeader[{}] failed: {}", i, r);
					return;
				}
				r = ::waveOutWrite(ch.device, &ch.headers[i], sizeof(WAVEHDR));
				if (r != MMSYSERR_NOERROR) {
					logger::warn("[FPCameraSettle][Fall][Audio] waveOutWrite[{}] failed: {}", i, r);
					::waveOutUnprepareHeader(ch.device, &ch.headers[i], sizeof(WAVEHDR));
					return;
				}
			}
			ch.playing = true;
		}

		void StopChannel(WaveChannel& ch)
		{
			if (!ch.opened) return;
			if (ch.playing) {
				ch.stopping.store(true, std::memory_order_relaxed);
				::waveOutReset(ch.device);
				for (int i = 0; i < 2; ++i)
					::waveOutUnprepareHeader(ch.device, &ch.headers[i], sizeof(WAVEHDR));
				ch.playing = false;
			}
		}

		void CloseChannel(WaveChannel& ch)
		{
			StopChannel(ch);
			if (ch.opened) {
				::waveOutClose(ch.device);
				ch.device = nullptr;
				ch.opened = false;
			}
		}
	}

	// ------ FallEffectManager audio methods ------
	// Volume is applied per-sample in the streaming callback (FillBuffer),
	// so SetVolume just stores the atomic float — no waveOutSetVolume calls.

	void FallEffectManager::PlayWind(float a_volume)
	{
		if (!g_wind.loaded) return;
		g_wind.volume.store(a_volume, std::memory_order_relaxed);
		if (!OpenChannel(g_wind)) return;
		PlayChannel(g_wind);
		windPlaying = true;
		windCurrentVolume = a_volume;
	}

	void FallEffectManager::PlayWhine(float a_volume)
	{
		if (!g_whine.loaded) return;
		g_whine.volume.store(a_volume, std::memory_order_relaxed);
		if (!OpenChannel(g_whine)) return;
		PlayChannel(g_whine);
		whinePlaying = true;
		whineCurrentVolume = a_volume;
	}

	void FallEffectManager::SetWindVolume(float a_volume)
	{
		g_wind.volume.store(a_volume, std::memory_order_relaxed);
		windCurrentVolume = a_volume;
	}

	void FallEffectManager::SetWhineVolume(float a_volume)
	{
		g_whine.volume.store(a_volume, std::memory_order_relaxed);
		whineCurrentVolume = a_volume;
	}

	void FallEffectManager::StopWind(bool a_immediate)
	{
		(void)a_immediate;
		StopChannel(g_wind);
		windPlaying = false;
		windCurrentVolume = 0.0f;
	}

	void FallEffectManager::StopWhine(bool a_immediate)
	{
		(void)a_immediate;
		StopChannel(g_whine);
		whinePlaying = false;
		whineCurrentVolume = 0.0f;
	}

	void FallEffectManager::PlayImpact(float a_volume)
	{
		if (!g_impact.loaded) return;
		g_impact.volume.store(a_volume, std::memory_order_relaxed);
		g_impact.looping.store(false, std::memory_order_relaxed);
		if (!OpenChannel(g_impact)) return;
		PlayChannel(g_impact);
	}

	void FallEffectManager::StopImpact()
	{
		StopChannel(g_impact);
	}

	void FallEffectManager::StopAllSounds()
	{
		StopChannel(g_wind);
		StopChannel(g_whine);
		StopChannel(g_impact);
		windPlaying  = false;
		whinePlaying = false;
		windCurrentVolume  = 0.0f;
		whineCurrentVolume = 0.0f;
		audioFading = false;
		audioPaused = false;
		fadeProgress = 0.0f;
	}

	void FallEffectManager::PauseAudio()
	{
		if (audioPaused) return;
		if (g_wind.opened  && g_wind.playing)   ::waveOutPause(g_wind.device);
		if (g_whine.opened && g_whine.playing)  ::waveOutPause(g_whine.device);
		if (g_impact.opened && g_impact.playing) ::waveOutPause(g_impact.device);
		audioPaused = true;
	}

	void FallEffectManager::ResumeAudio()
	{
		if (!audioPaused) return;
		if (g_wind.opened  && g_wind.playing)   ::waveOutRestart(g_wind.device);
		if (g_whine.opened && g_whine.playing)  ::waveOutRestart(g_whine.device);
		if (g_impact.opened && g_impact.playing) ::waveOutRestart(g_impact.device);
		audioPaused = false;
	}

	void FallEffectManager::ReloadAudio()
	{
		CloseChannel(g_wind);
		CloseChannel(g_whine);
		CloseChannel(g_impact);
		g_wind.loaded   = false;
		g_whine.loaded  = false;
		g_impact.loaded = false;
		g_wind.sourcePcm.clear();
		g_whine.sourcePcm.clear();
		g_impact.sourcePcm.clear();
		windPlaying  = false;
		whinePlaying = false;
		windCurrentVolume  = 0.0f;
		whineCurrentVolume = 0.0f;
		audioFading = false;
		audioPaused = false;
		fadeProgress = 0.0f;
		PreloadAudioFiles();
		logger::info("[FPCameraSettle][Fall][Audio] Reload requested - channels re-loaded");
	}

	void FallEffectManager::PreloadAudioFiles()
	{
		if (!g_wind.loaded) {
			windFilePresent = LoadChannel(g_wind, WIND_FILE);
		}
		if (!g_whine.loaded) {
			whineFilePresent = LoadChannel(g_whine, WHINE_FILE);
		}
		if (!g_impact.loaded) {
			g_impact.looping.store(false, std::memory_order_relaxed);
			impactFilePresent = LoadChannel(g_impact, IMPACT_FILE);
		}
		if (g_wind.loaded && !g_wind.opened) {
			OpenChannel(g_wind);
		}
		if (g_whine.loaded && !g_whine.opened) {
			OpenChannel(g_whine);
		}
		if (g_impact.loaded && !g_impact.opened) {
			OpenChannel(g_impact);
		}
		logger::info("[FPCameraSettle][Fall][Audio] Preload: wind={}, whine={}, impact={}",
			windFilePresent ? "OK" : "MISSING",
			whineFilePresent ? "OK" : "MISSING",
			impactFilePresent ? "OK" : "MISSING");
	}

	void FallEffectManager::RefreshAudioFileExistence()
	{
		std::wstring wp = ResolveSoundPath(WIND_FILE);
		std::wstring hp = ResolveSoundPath(WHINE_FILE);
		std::wstring ip = ResolveSoundPath(IMPACT_FILE);
		windFilePresent   = (::GetFileAttributesW(wp.c_str()) != INVALID_FILE_ATTRIBUTES);
		whineFilePresent  = (::GetFileAttributesW(hp.c_str()) != INVALID_FILE_ATTRIBUTES);
		impactFilePresent = (::GetFileAttributesW(ip.c_str()) != INVALID_FILE_ATTRIBUTES);
	}

	void FallEffectManager::TickAudioFade(float a_delta)
	{
		if (!audioFading) return;

		auto* settings = Settings::GetSingleton();
		float fadeDuration = std::max(0.05f, settings->fallAudioFadeOut);

		fadeProgress += a_delta / fadeDuration;
		float t = Clamp01(fadeProgress);
		float k = 1.0f - SmoothStep(t);  // ease-out

		if (windPlaying)  SetWindVolume(fadeStartWindVol * k);
		if (whinePlaying) SetWhineVolume(fadeStartWhineVol * k);

		if (t >= 1.0f) {
			StopWind(true);
			StopWhine(true);
			audioFading = false;
		}
	}

	// -----------------------------------------------------------------------
	// IMOD helpers
	// -----------------------------------------------------------------------
	namespace
	{
		// Deep-copy a donor NiFloatInterpolator and make it "posed" — constant
		// value 1.0 with no animation keys. The engine evaluates
		// `interpolator_value * instance->strength` each frame; a posed
		// interpolator that always returns 1.0 lets instance->strength be
		// the direct magnitude control. Without this, the donor's keyframe
		// curve animates to 0 and the effect vanishes after a few seconds.
		RE::NiPointer<RE::NiFloatInterpolator> MakePosedInterpolator(
			RE::NiFloatInterpolator* a_donor)
		{
			if (!a_donor) return nullptr;

			RE::NiPointer<RE::NiObject> copy;
			a_donor->CreateDeepCopy(copy);
			if (!copy) {
				logger::warn("[FPCameraSettle][Fall][IMOD] CreateDeepCopy failed for interpolator");
				return nullptr;
			}

			auto* posed = static_cast<RE::NiFloatInterpolator*>(copy.get());
			posed->floatData  = nullptr;
			posed->floatValue = 1.0f;
			posed->lastIndex  = 0;
			return RE::NiPointer<RE::NiFloatInterpolator>(posed);
		}

		RE::NiPointer<RE::NiColorInterpolator> MakePosedColorInterpolator(
			RE::NiColorInterpolator* a_donor, const RE::NiColorA& a_value)
		{
			if (!a_donor) return nullptr;

			RE::NiPointer<RE::NiObject> copy;
			a_donor->CreateDeepCopy(copy);
			if (!copy) {
				logger::warn("[FPCameraSettle][Fall][IMOD] CreateDeepCopy failed for color interpolator");
				return nullptr;
			}

			auto* posed = static_cast<RE::NiColorInterpolator*>(copy.get());
			posed->colorData  = nullptr;
			posed->colorValue = a_value;
			posed->lastIndex  = 0;
			return RE::NiPointer<RE::NiColorInterpolator>(posed);
		}

		RE::TESImageSpaceModifier* CloneFadeIMOD(RE::TESImageSpaceModifier* a_donor,
		                                          const char* a_editorID)
		{
			const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESImageSpaceModifier>();
			if (!factory) return nullptr;

			auto* clone = factory->Create();
			if (!clone) return nullptr;

			clone->data          = a_donor->data;
			clone->data.duration = 99999.0f;

			clone->formFlags     = a_donor->formFlags;
			clone->formType      = a_donor->formType;

			clone->doubleVisionStrength = nullptr;
			clone->radialBlur   = {};
			clone->blurRadius   = nullptr;
			clone->motionBlurStrength = nullptr;

			if (a_donor->tintColor) {
				auto posed = MakePosedColorInterpolator(
					a_donor->tintColor.get(), RE::NiColorA(0.0f, 0.0f, 0.0f, 1.0f));
				if (posed) {
					clone->tintColor = posed;
					logger::info("[FPCameraSettle][Fall][IMOD] Fade tintColor posed to black (from donor tintColor)");
				}
			}
			if (a_donor->fadeColor) {
				auto posed = MakePosedColorInterpolator(
					a_donor->fadeColor.get(), RE::NiColorA(0.0f, 0.0f, 0.0f, 1.0f));
				if (posed) {
					clone->fadeColor = posed;
					logger::info("[FPCameraSettle][Fall][IMOD] Fade fadeColor posed to black (from donor fadeColor)");
				}
			}

			clone->SetFormEditorID(a_editorID);
			return clone;
		}

		RE::TESImageSpaceModifier* CloneIMOD(RE::TESImageSpaceModifier* a_donor,
		                                      const char* a_editorID,
		                                      bool a_keepDoubleVision,
		                                      bool a_keepRadialBlur)
		{
			const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESImageSpaceModifier>();
			if (!factory) return nullptr;

			auto* clone = factory->Create();
			if (!clone) return nullptr;

			clone->data       = a_donor->data;
			clone->data.duration = 99999.0f;

			clone->formFlags  = a_donor->formFlags;
			clone->formType   = a_donor->formType;
			clone->hdr        = a_donor->hdr;
			clone->bloom      = a_donor->bloom;
			clone->cinematic  = a_donor->cinematic;
			clone->dof        = a_donor->dof;
			clone->fadeColor  = a_donor->fadeColor;
			clone->tintColor  = a_donor->tintColor;
			clone->blurRadius = a_donor->blurRadius;
			clone->motionBlurStrength = a_donor->motionBlurStrength;

			if (a_keepDoubleVision && a_donor->doubleVisionStrength) {
				auto posed = MakePosedInterpolator(a_donor->doubleVisionStrength.get());
				if (posed) {
					clone->doubleVisionStrength = posed;
					logger::info("[FPCameraSettle][Fall][IMOD] DV interpolator posed (deep-copied, keys stripped)");
				} else {
					clone->doubleVisionStrength = a_donor->doubleVisionStrength;
					logger::warn("[FPCameraSettle][Fall][IMOD] DV interpolator fallback to shared (deep copy failed)");
				}
			} else {
				clone->doubleVisionStrength = nullptr;
			}

			if (a_keepRadialBlur) {
				clone->radialBlur = a_donor->radialBlur;
				if (a_donor->radialBlur.strength) {
					auto posed = MakePosedInterpolator(a_donor->radialBlur.strength.get());
					if (posed) {
						clone->radialBlur.strength = posed;
						logger::info("[FPCameraSettle][Fall][IMOD] MB strength interpolator posed");
					}
				}
			} else {
				clone->radialBlur = {};
			}

			clone->SetFormEditorID(a_editorID);
			return clone;
		}
	}

	void FallEffectManager::EnsureIMODs()
	{
		if (imodInitialized) return;
		imodInitialized = true;

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			logger::warn("[FPCameraSettle][Fall][IMOD] TESDataHandler not available");
			return;
		}

		auto& imodArray = dataHandler->GetFormArray<RE::TESImageSpaceModifier>();
		logger::info("[FPCameraSettle][Fall][IMOD] Scanning {} IMODs for donors...", imodArray.size());

		RE::TESImageSpaceModifier* sourceWithDouble = nullptr;
		RE::TESImageSpaceModifier* sourceWithBlur   = nullptr;
		RE::TESImageSpaceModifier* sourceWithTint   = nullptr;
		RE::TESImageSpaceModifier* vanillaFade      = nullptr;
		std::size_t scannedCount = 0, doubleCount = 0, blurCount = 0, tintCount = 0;

		for (auto* imod : imodArray) {
			if (!imod) continue;
			++scannedCount;
			if (imod->doubleVisionStrength) {
				++doubleCount;
				if (!sourceWithDouble) sourceWithDouble = imod;
			}
			if (imod->radialBlur.strength) {
				++blurCount;
				if (!sourceWithBlur) sourceWithBlur = imod;
			}
			if (imod->tintColor || imod->fadeColor) {
				++tintCount;
				if (!sourceWithTint) sourceWithTint = imod;
			}

			if (!vanillaFade) {
				const char* eid = imod->GetFormEditorID();
				if (eid) {
					std::string_view sv(eid);
					if (sv == "FadeToBlackHoldImod" || sv == "FadeToBlackImod" ||
						sv == "FadeToBlackBackImod")
					{
						vanillaFade = imod;
					}
				}
			}
		}

		const char* dvID  = sourceWithDouble ? sourceWithDouble->GetFormEditorID() : nullptr;
		const char* blrID = sourceWithBlur   ? sourceWithBlur->GetFormEditorID()   : nullptr;
		const char* tntID = sourceWithTint   ? sourceWithTint->GetFormEditorID()   : nullptr;
		const char* vfID  = vanillaFade      ? vanillaFade->GetFormEditorID()      : nullptr;
		logger::info("[FPCameraSettle][Fall][IMOD] Scan: {} scanned, {} doubleVision, {} radialBlur, {} tint/fade. donors: DV={} (0x{:08X}), MB={} (0x{:08X}), Tint={} (0x{:08X}), VanillaFade={} (0x{:08X})",
			scannedCount, doubleCount, blurCount, tintCount,
			dvID  ? dvID  : "<none>", sourceWithDouble ? sourceWithDouble->GetFormID() : 0u,
			blrID ? blrID : "<none>", sourceWithBlur   ? sourceWithBlur->GetFormID()   : 0u,
			tntID ? tntID : "<none>", sourceWithTint   ? sourceWithTint->GetFormID()   : 0u,
			vfID  ? vfID  : "<none>", vanillaFade      ? vanillaFade->GetFormID()      : 0u);

		if (sourceWithDouble) {
			fallImodDV = CloneIMOD(sourceWithDouble, "FPCameraSettleFallDV", true, false);
			if (fallImodDV) {
				imodArray.push_back(fallImodDV);
				logger::info("[FPCameraSettle][Fall][IMOD] DV clone OK. FormID=0x{:08X}, donor={}, duration={:.0f}",
					fallImodDV->GetFormID(),
					dvID ? dvID : "<unnamed>",
					fallImodDV->data.duration);
			}
		} else {
			logger::warn("[FPCameraSettle][Fall][IMOD] No donor with doubleVisionStrength");
		}

		if (sourceWithBlur) {
			fallImodMB = CloneIMOD(sourceWithBlur, "FPCameraSettleFallMB", false, true);
			if (fallImodMB) {
				imodArray.push_back(fallImodMB);
				logger::info("[FPCameraSettle][Fall][IMOD] MB clone OK. FormID=0x{:08X}, donor={}, duration={:.0f}",
					fallImodMB->GetFormID(),
					blrID ? blrID : "<unnamed>",
					fallImodMB->data.duration);
			}
		} else {
			logger::warn("[FPCameraSettle][Fall][IMOD] No donor with radialBlur.strength");
		}

		RE::TESImageSpaceModifier* fadeDonor = vanillaFade ? vanillaFade : sourceWithTint;
		const char* fadeDonorID = fadeDonor ? fadeDonor->GetFormEditorID() : nullptr;
		if (fadeDonor) {
			fallImodFade = CloneFadeIMOD(fadeDonor, "FPCameraSettleFallFade");
			if (fallImodFade) {
				imodArray.push_back(fallImodFade);
				logger::info("[FPCameraSettle][Fall][IMOD] Fade-to-black clone OK. FormID=0x{:08X}, donor={} (0x{:08X})",
					fallImodFade->GetFormID(),
					fadeDonorID ? fadeDonorID : "<unnamed>",
					fadeDonor->GetFormID());
			}
		} else {
			logger::warn("[FPCameraSettle][Fall][IMOD] No donor for fade-to-black (no vanilla FadeToBlack IMOD and no tintColor/fadeColor source)");
		}
	}

	void FallEffectManager::StopAllIMODs()
	{
		if (dvActive && fallImodDV) {
			RE::ImageSpaceModifierInstanceForm::Stop(fallImodDV);
		}
		if (mbActive && fallImodMB) {
			RE::ImageSpaceModifierInstanceForm::Stop(fallImodMB);
		}
		if (fadeActive && fallImodFade) {
			RE::ImageSpaceModifierInstanceForm::Stop(fallImodFade);
		}
		fallImodDVInstance   = nullptr;
		fallImodMBInstance   = nullptr;
		fallImodFadeInstance = nullptr;
		dvActive   = false;
		mbActive   = false;
		fadeActive  = false;
	}

	void FallEffectManager::ApplyFadeToBlack(float a_strength)
	{
		if (!fallImodFade) return;

		bool wantFade = (a_strength > 0.001f);
		if (wantFade && !fadeActive) {
			fallImodFadeInstance = RE::ImageSpaceModifierInstanceForm::Trigger(fallImodFade, a_strength, nullptr);
			fadeActive = (fallImodFadeInstance != nullptr);
		} else if (!wantFade && fadeActive) {
			RE::ImageSpaceModifierInstanceForm::Stop(fallImodFade);
			fallImodFadeInstance = nullptr;
			fadeActive = false;
		}
		if (fadeActive && fallImodFadeInstance) {
			fallImodFadeInstance->strength = a_strength;
		}
	}

	void FallEffectManager::ApplyIMODStrengths(float a_doubleVision, float a_motionBlur)
	{
		// === Double vision channel ===
		if (fallImodDV) {
			bool wantDV = (a_doubleVision > 0.001f);
			if (wantDV && !dvActive) {
				fallImodDVInstance = RE::ImageSpaceModifierInstanceForm::Trigger(fallImodDV, a_doubleVision, nullptr);
				dvActive = (fallImodDVInstance != nullptr);
			} else if (!wantDV && dvActive) {
				RE::ImageSpaceModifierInstanceForm::Stop(fallImodDV);
				fallImodDVInstance = nullptr;
				dvActive = false;
			}
			if (dvActive && fallImodDVInstance) {
				fallImodDVInstance->strength = a_doubleVision;
			}
		}

		// === Motion / radial blur channel ===
		if (fallImodMB) {
			bool wantMB = (a_motionBlur > 0.001f);
			if (wantMB && !mbActive) {
				fallImodMBInstance = RE::ImageSpaceModifierInstanceForm::Trigger(fallImodMB, a_motionBlur, nullptr);
				mbActive = (fallImodMBInstance != nullptr);
			} else if (!wantMB && mbActive) {
				RE::ImageSpaceModifierInstanceForm::Stop(fallImodMB);
				fallImodMBInstance = nullptr;
				mbActive = false;
			}
			if (mbActive && fallImodMBInstance) {
				fallImodMBInstance->strength = a_motionBlur;
			}
		}

		lastDoubleVision = a_doubleVision;
		lastMotionBlur   = a_motionBlur;
	}

	// -----------------------------------------------------------------------
	// Initialization / reset
	// -----------------------------------------------------------------------
	void FallEffectManager::Initialize()
	{
		EnsureIMODs();
		PreloadAudioFiles();
	}

	void FallEffectManager::Reset()
	{
		currentPhase = Phase::Inactive;
		fallTime = 0.0f;
		landingTime = 0.0f;
		intensity01 = 0.0f;
		prevZ = 0.0f;
		estVelZ = 0.0f;
		hadValidPrevZ = false;
		wasInAir = false;
		wasBlocked = false;
		lowVelocityTime = 0.0f;
		peakPhase = Phase::Inactive;
		positionOffset = { 0.0f, 0.0f, 0.0f };
		rotationOffset = { 0.0f, 0.0f, 0.0f };
		fovOffset = 0.0f;
		shakePhase = 0.0f;
		fovPhase = 0.0f;
		windTargetVolume = 0.0f;
		whineTargetVolume = 0.0f;
		fatalLandingTime = 0.0f;
		fatalWhineVolume = 0.0f;
		fatalImpactPlayed = false;

		StopAllSounds();
		StopAllIMODs();
	}

	void FallEffectManager::EnterFatalLanding()
	{
		auto* settings = Settings::GetSingleton();

		currentPhase = Phase::FatalLanding;
		fatalLandingTime = 0.0f;
		fatalImpactPlayed = false;

		if (windPlaying) {
			StopWind(true);
		}

		float whineSpike = settings->fallMasterVolume *
			settings->fallWhineMaxVolume * settings->fallFatalWhineBoost;
		if (whinePlaying) {
			SetWhineVolume(whineSpike);
		} else if (settings->fallWhineEnabled) {
			PlayWhine(whineSpike);
		}
		fatalWhineVolume = whineSpike;

		float impactVol = settings->fallFatalImpactVolume;
		if (impactVol > 0.001f) {
			PlayImpact(impactVol);
			fatalImpactPlayed = true;
		}

		audioFading = false;
		ApplyIMODStrengths(0.0f, 0.0f);

		positionOffset = { 0.0f, 0.0f, 0.0f };
		rotationOffset = { 0.0f, 0.0f, 0.0f };
		fovOffset = 0.0f;

		logger::info("[FPCameraSettle][Fall] -> FatalLanding (player died from fall)");
	}

	void FallEffectManager::CheckDeathTransition()
	{
		if (currentPhase == Phase::FatalLanding || currentPhase == Phase::Inactive)
			return;

		auto* settings = Settings::GetSingleton();
		if (!settings->fallFatalEnabled) return;
		if (peakPhase < Phase::Phase2) return;

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!IsPlayerFatallyLanded(player)) return;

		EnterFatalLanding();
	}

	// -----------------------------------------------------------------------
	// Detection: only trigger if grounded conditions are NOT in effect
	// (flying, swimming, on mount, tcl/no-clip, or known paraglider grab).
	// -----------------------------------------------------------------------
	bool FallEffectManager::IsBlockingState(RE::PlayerCharacter* a_player) const
	{
		if (!a_player) return true;

		auto* actorState = a_player->AsActorState();
		if (!actorState) return true;

		// Console toggle-collision (tcl) leaves the CharController null/disabled;
		// in that state the player floats freely, IsInMidair() can give a false
		// positive, and there's no real fall happening.
		if (!a_player->GetCharController()) return true;

		if (a_player->IsOnMount()) return true;
		if (actorState->IsSwimming()) return true;
		if (actorState->IsFlying()) return true;

		bool isGliding = false;
		if (a_player->GetGraphVariableBool("bIsGliding", isGliding) && isGliding) return true;
		if (a_player->GetGraphVariableBool("IsParaglider", isGliding) && isGliding) return true;
		if (a_player->GetGraphVariableBool("bParagliderActive", isGliding) && isGliding) return true;
		if (a_player->GetGraphVariableBool("IsParagliding", isGliding) && isGliding) return true;
		if (a_player->GetGraphVariableBool("bIsParagliding", isGliding) && isGliding) return true;
		if (a_player->GetGraphVariableBool("isParagliding", isGliding) && isGliding) return true;

		return false;
	}

	bool FallEffectManager::ShouldTriggerFall(RE::PlayerCharacter* a_player, Settings* a_settings, float a_delta)
	{
		(void)a_delta;
		if (!a_player || !a_settings) return false;
		if (IsBlockingState(a_player)) return false;

		// Must be in air for the effect to start
		if (!a_player->IsInMidair()) return false;

		bool airTimeOk  = (fallTime >= a_settings->fallTriggerTime);
		// estVelZ is positive when descending (we negate raw velocity below)
		bool velocityOk = (estVelZ >= a_settings->fallTriggerVelocity);

		if (a_settings->fallRequireBothConditions) {
			return airTimeOk && velocityOk;
		}
		return airTimeOk || velocityOk;
	}

	bool FallEffectManager::IsPlayerFatallyLanded(RE::PlayerCharacter* a_player) const
	{
		if (!a_player) return false;
		if (a_player->IsDead()) return true;
		auto* avo = a_player->AsActorValueOwner();
		if (avo && avo->GetActorValue(RE::ActorValue::kHealth) <= 0.0f) return true;
		return false;
	}

	// -----------------------------------------------------------------------
	// Procedural shake: combine a smooth sine and a value-noise channel.
	// We compute three independent channels (pitch/yaw/roll) by sampling at
	// different phase offsets, weighted by the sine/noise mix.
	// -----------------------------------------------------------------------
	float FallEffectManager::Noise1D(float x) const
	{
		// Cheap deterministic value noise - integer hash + smooth interp.
		float xi = std::floor(x);
		float xf = x - xi;
		auto hash = [](int n) {
			n = (n << 13) ^ n;
			int v = (n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff;
			return 1.0f - static_cast<float>(v) / 1073741824.0f;  // -1..1
		};
		float a = hash(static_cast<int>(xi));
		float b = hash(static_cast<int>(xi) + 1);
		float u = xf * xf * (3.0f - 2.0f * xf);
		return a + (b - a) * u;
	}

	void FallEffectManager::ComputeShake(float a_delta)
	{
		auto* settings = Settings::GetSingleton();
		positionOffset = { 0.0f, 0.0f, 0.0f };
		rotationOffset = { 0.0f, 0.0f, 0.0f };
		fovOffset = 0.0f;

		if (currentPhase == Phase::Inactive) {
			shakePhase = 0.0f;
			fovPhase = 0.0f;
			return;
		}

		// Shake envelope: ramps 0→1 over fallShakeFadeIn from fall start,
		// then fades out on landing. Uses the global fade curve.
		float phaseEnv = 0.0f;
		if (currentPhase == Phase::Landing) {
			float ft = Clamp01(landingTime / 0.25f);
			phaseEnv = 1.0f - SmoothStep(ft);
		} else {
			float fadeIn = std::max(0.01f, settings->fallShakeFadeIn);
			phaseEnv = ApplyFadeCurve(fallTime / fadeIn, settings->fallFadeCurve);
		}

		// Optional velocity scaling (more shake the faster you fall)
		float velScale = 1.0f;
		if (settings->fallShakeScaleByVelocity) {
			velScale = 0.6f + 0.4f * VelocityIntensity(estVelZ, settings->fallTriggerVelocity);
		}

		intensity01 = phaseEnv;
		float master = settings->fallShakeIntensity * phaseEnv * velScale;

		if (!settings->fallShakeEnabled || master <= 0.0001f) {
			return;
		}

		// Advance phases (continuous, never resets, smooth blend in/out)
		shakePhase += a_delta * settings->fallShakeFrequency * 2.0f * PI;
		if (shakePhase > 1000.0f * PI) {
			shakePhase = std::fmod(shakePhase, 2.0f * PI);
		}

		// Sine channels (different phase offsets per axis)
		float sx = std::sin(shakePhase);
		float sy = std::sin(shakePhase * 1.37f + 1.7f);
		float sz = std::sin(shakePhase * 0.91f + 3.3f);

		// Noise channels (sampled at phase * a small factor so it's not too jittery)
		float nx = Noise1D(shakePhase * 0.5f);
		float ny = Noise1D(shakePhase * 0.5f + 17.3f);
		float nz = Noise1D(shakePhase * 0.5f + 31.9f);

		float noiseMix = settings->fallShakeNoiseAmount;
		float sineMix  = 1.0f - noiseMix;

		float chX = sx * sineMix + nx * noiseMix;
		float chY = sy * sineMix + ny * noiseMix;
		float chZ = sz * sineMix + nz * noiseMix;

		// === Rotation (degrees -> radians applied at end) ===
		// Base rotation amplitudes (in degrees) at full intensity.
		// These are tuned to feel "Mirror's Edge" disorienting without breaking
		// first-person arms alignment too aggressively.
		constexpr float BASE_PITCH_DEG = 1.6f;
		constexpr float BASE_YAW_DEG   = 1.1f;
		constexpr float BASE_ROLL_DEG  = 1.4f;

		float rotMaster = master * settings->fallShakeRotScale;

		float pitchDeg = settings->fallShakeAffectPitch ? (chX * BASE_PITCH_DEG * rotMaster) : 0.0f;
		float yawDeg   = settings->fallShakeAffectYaw   ? (chY * BASE_YAW_DEG   * rotMaster) : 0.0f;
		float rollDeg  = settings->fallShakeAffectRoll  ? (chZ * BASE_ROLL_DEG  * rotMaster) : 0.0f;

		// Phase 3 downward bias on pitch (head wants to look down)
		if (currentPhase == Phase::Phase3 && settings->fallShakeAffectPitch) {
			pitchDeg += settings->fallShakeDownwardBias * Clamp01(phaseEnv);
		}

		rotationOffset.x = pitchDeg * DEG_TO_RAD;
		rotationOffset.y = rollDeg  * DEG_TO_RAD;  // y is roll in this codebase
		rotationOffset.z = yawDeg   * DEG_TO_RAD;  // z is yaw

		// === Position (units) ===
		if (settings->fallShakeAffectPosition) {
			constexpr float BASE_POS = 0.6f;
			float posMaster = master * settings->fallShakePosScale;
			positionOffset.x = chY * BASE_POS * posMaster;
			positionOffset.y = chX * BASE_POS * posMaster * 0.6f;
			positionOffset.z = chZ * BASE_POS * posMaster * 0.5f;
		}

		// === FOV oscillation (Phase 3 only by default; ramp out on landing) ===
		if (settings->fallFovEnabled && (currentPhase == Phase::Phase3 || currentPhase == Phase::Landing)) {
			fovPhase += a_delta * settings->fallFovOscFrequency * 2.0f * PI;
			if (fovPhase > 1000.0f * PI) fovPhase = std::fmod(fovPhase, 2.0f * PI);
			float fovEnv = (currentPhase == Phase::Landing)
				? Clamp01(1.0f - landingTime / 0.4f)
				: 1.0f;
			fovOffset = std::sin(fovPhase) * settings->fallFovOscAmplitude * fovEnv * velScale;
		}
	}

	// -----------------------------------------------------------------------
	// Per-frame update
	// -----------------------------------------------------------------------
	void FallEffectManager::Update(float a_delta)
	{
		auto* settings = Settings::GetSingleton();
		auto* player   = RE::PlayerCharacter::GetSingleton();

		if (!settings || !player) return;

		// Master toggles - if disabled, ensure clean state
		if (!settings->enabled || !settings->fallEffectEnabled) {
			if (currentPhase != Phase::Inactive || windPlaying || whinePlaying || dvActive || mbActive) {
				Reset();
			}
			return;
		}

		// First-person check is done in CameraSettleManager before calling us;
		// but defensively reset audio if we somehow exit FP mid-fall.
		// Exception: FatalLanding must survive the death camera switch.
		auto* camera = RE::PlayerCamera::GetSingleton();
		bool inFP = camera && camera->IsInFirstPerson();
		if (!inFP && currentPhase != Phase::FatalLanding) {
			if (currentPhase != Phase::Inactive) Reset();
			return;
		}

		// Estimate downward velocity from Z position delta.
		// Z in Skyrim increases upward; descending velocity is positive in our frame.
		float curZ = player->GetPosition().z;
		bool isInAir = player->IsInMidair();

		if (hadValidPrevZ && a_delta > 1e-5f) {
			float dz = prevZ - curZ;     // positive when descending
			float instVel = dz / a_delta;
			// Smooth a bit to avoid spikes when crossing z-fights
			estVelZ = estVelZ * 0.6f + instVel * 0.4f;
		} else {
			estVelZ = 0.0f;
		}
		prevZ = curZ;
		hadValidPrevZ = true;

		// Only count time as "fall time" while genuinely falling and unblocked.
		// Velocity gate: don't accumulate fallTime while descending slowly
		// (e.g. paragliding), even if IsBlockingState can't detect the mod.
		bool blocked = IsBlockingState(player);
		float velGateMin = settings->fallTriggerVelocity * 0.25f;
		bool descendingFast = (estVelZ > velGateMin);

		// Detect blocked→unblocked transitions (paraglider deactivated mid-air)
		if (wasBlocked && !blocked && isInAir) {
			fallTime = 0.0f;
			shakePhase = 0.0f;
			fovPhase = 0.0f;
			lowVelocityTime = 0.0f;
			estVelZ = 0.0f;
			hadValidPrevZ = false;
			if (currentPhase != Phase::Inactive && currentPhase != Phase::Landing &&
				currentPhase != Phase::FatalLanding)
			{
				currentPhase = Phase::Inactive;
			}
			if (settings->debugLogging) {
				logger::info("[FPCameraSettle][Fall] Blocked->unblocked in air: reset fallTime");
			}
		}

		if (isInAir && !blocked && descendingFast) {
			if (!wasInAir || (wasBlocked && !blocked)) {
				fallTime = 0.0f;
				shakePhase = 0.0f;
				fovPhase = 0.0f;
			}
			fallTime += a_delta;
			lowVelocityTime = 0.0f;
		} else if (isInAir && !blocked && !descendingFast) {
			lowVelocityTime += a_delta;
		} else {
			fallTime = 0.0f;
			lowVelocityTime = 0.0f;
		}

		// === Phase machine ===
		Phase prevPhase = currentPhase;

		if (currentPhase == Phase::Inactive) {
			if (isInAir && !blocked && ShouldTriggerFall(player, settings, a_delta)) {
				currentPhase = Phase::Phase1;
				peakPhase = Phase::Phase1;
				landingTime = 0.0f;
				audioFading = false;
				if (settings->debugLogging) {
					logger::info("[FPCameraSettle][Fall] -> Phase1 (airTime={:.2f}s, velZ={:.0f})",
						fallTime, estVelZ);
				}
			}
		} else if (currentPhase == Phase::Phase1 || currentPhase == Phase::Phase2 || currentPhase == Phase::Phase3) {
			bool shouldLand = !isInAir || blocked;

			// Low velocity exit: if velocity drops below the gate for >0.3s
			// while still in air (e.g. paraglider re-engaged), trigger landing.
			if (!shouldLand && isInAir && !blocked && lowVelocityTime > 0.3f) {
				shouldLand = true;
			}

			if (shouldLand) {
				currentPhase = Phase::Landing;
				landingTime = 0.0f;
				audioFading = true;
				fadeProgress = 0.0f;
				fadeStartWindVol = windCurrentVolume;
				fadeStartWhineVol = whineCurrentVolume;
				if (settings->debugLogging) {
					logger::info("[FPCameraSettle][Fall] -> Landing (airTime={:.2f}s, blocked={}, inAir={}, lowVelTime={:.2f})",
						fallTime, blocked, isInAir, lowVelocityTime);
				}
			} else {
				float t1 = settings->fallPhase1Duration;
				float t2 = t1 + settings->fallPhase2Duration;
				if (fallTime >= t2) {
					currentPhase = Phase::Phase3;
					peakPhase = Phase::Phase3;
				} else if (fallTime >= t1) {
					currentPhase = Phase::Phase2;
					if (peakPhase < Phase::Phase2) peakPhase = Phase::Phase2;
				} else {
					currentPhase = Phase::Phase1;
				}
			}
		} else if (currentPhase == Phase::Landing) {
			landingTime += a_delta;

			// Check for fatal fall (player died from landing damage).
			// Only trigger if the fall reached at least Phase2 intensity.
			// Keep checking throughout the entire Landing phase — fall damage
			// can be delayed several frames by the engine.
			if (settings->fallFatalEnabled &&
				peakPhase >= Phase::Phase2 && IsPlayerFatallyLanded(player))
			{
				EnterFatalLanding();
			} else {
				float fadeT = std::max(0.05f, settings->fallAudioFadeOut);
				if (landingTime > std::max(0.4f, fadeT) && !audioFading) {
					currentPhase = Phase::Inactive;
					positionOffset = { 0.0f, 0.0f, 0.0f };
					rotationOffset = { 0.0f, 0.0f, 0.0f };
					fovOffset = 0.0f;
				}
			}
		} else if (currentPhase == Phase::FatalLanding) {
			fatalLandingTime += a_delta;

			// Decay whine from spike to silence
			float decayDur = std::max(0.1f, settings->fallFatalWhineDecay);
			float whineT = Clamp01(fatalLandingTime / decayDur);
			float whineK = 1.0f - SmoothStep(whineT);
			float whineVol = fatalWhineVolume * whineK;
			if (whinePlaying) {
				if (whineVol < 0.001f) {
					StopWhine(true);
				} else {
					SetWhineVolume(whineVol);
				}
			}

			// Drive fade-to-black IMOD
			float fadeInDur  = std::max(0.01f, settings->fallFatalFadeInTime);
			float holdEnd    = fadeInDur + settings->fallFatalBlackDuration;
			float fadeOutDur = std::max(0.1f, settings->fallFatalFadeOutTime);
			float totalEnd   = holdEnd + fadeOutDur;

			float fadeStrength = 0.0f;
			if (fatalLandingTime < fadeInDur) {
				fadeStrength = Clamp01(fatalLandingTime / fadeInDur);
			} else if (fatalLandingTime < holdEnd) {
				fadeStrength = 1.0f;
			} else if (fatalLandingTime < totalEnd) {
				fadeStrength = 1.0f - Clamp01((fatalLandingTime - holdEnd) / fadeOutDur);
			} else {
				fadeStrength = 0.0f;
			}
			ApplyFadeToBlack(fadeStrength);

			// No camera shake during fatal landing
			positionOffset = { 0.0f, 0.0f, 0.0f };
			rotationOffset = { 0.0f, 0.0f, 0.0f };
			fovOffset = 0.0f;

			// End fatal landing after everything completes
			if (fatalLandingTime >= totalEnd) {
				currentPhase = Phase::Inactive;
				ApplyFadeToBlack(0.0f);
				if (whinePlaying) StopWhine(true);
				StopImpact();
				if (settings->debugLogging) {
					logger::info("[FPCameraSettle][Fall] FatalLanding complete");
				}
			}
		}

		if (settings->debugLogging && prevPhase != currentPhase) {
			logger::info("[FPCameraSettle][Fall] Phase {} -> {} (fallTime={:.2f}s, velZ={:.0f}, intensity={:.2f})",
				static_cast<int>(prevPhase), static_cast<int>(currentPhase),
				fallTime, estVelZ, intensity01);
		}

		wasInAir = isInAir;
		wasBlocked = blocked;

		// === Compute shake offsets for this frame ===
		if (currentPhase != Phase::FatalLanding) {
			ComputeShake(a_delta);
		}

		// === Drive audio ===
		if (currentPhase != Phase::FatalLanding) {
			float velScale = settings->fallAudioVolumeByVelocity
				? (0.5f + 0.5f * VelocityIntensity(estVelZ, settings->fallTriggerVelocity))
				: 1.0f;

			float windVol = 0.0f;
			float whineVol = 0.0f;

			if (settings->fallAudioEnabled && (currentPhase == Phase::Phase1 ||
				currentPhase == Phase::Phase2 || currentPhase == Phase::Phase3))
			{
				int curve = settings->fallFadeCurve;
				if (settings->fallWindEnabled) {
					float fadeIn = std::max(0.001f, settings->fallWindFadeIn);
					float windEnv = ApplyFadeCurve(fallTime / fadeIn, curve);
					windVol = settings->fallMasterVolume * settings->fallWindMaxVolume * windEnv * velScale;
				}
				if (settings->fallWhineEnabled) {
					if (fallTime <= settings->fallPhase1Duration) {
						whineVol = 0.0f;
					} else {
						float fadeIn = std::max(0.001f, settings->fallWhineFadeIn);
						float t = (fallTime - settings->fallPhase1Duration) / fadeIn;
						whineVol = settings->fallMasterVolume * settings->fallWhineMaxVolume *
							ApplyFadeCurve(t, curve) * velScale;
					}
				}
			}

			windTargetVolume = windVol;
			whineTargetVolume = whineVol;

			if (currentPhase != Phase::Landing && currentPhase != Phase::Inactive) {
				if (settings->fallWindEnabled && windVol > 0.001f) {
					if (!windPlaying) {
						PlayWind(windVol);
					} else {
						SetWindVolume(windVol);
					}
				} else if (windPlaying && !settings->fallWindEnabled) {
					StopWind(true);
				}

				if (settings->fallWhineEnabled && whineVol > 0.001f) {
					if (!whinePlaying) {
						PlayWhine(whineVol);
					} else {
						SetWhineVolume(whineVol);
					}
				} else if (whinePlaying && !settings->fallWhineEnabled) {
					StopWhine(true);
				}
			} else if (currentPhase == Phase::Landing) {
				TickAudioFade(a_delta);
			}
		}

		// === Drive IMOD (visual effects) ===
		if (currentPhase != Phase::FatalLanding) {
			float dvStrength = 0.0f;
			float mbStrength = 0.0f;
			int   curve = settings->fallFadeCurve;

			if (currentPhase == Phase::Phase2 || currentPhase == Phase::Phase3) {
				float dvTime = fallTime - settings->fallPhase1Duration;
				if (dvTime > 0.0f) {
					float dvFade = std::max(0.01f, settings->fallDoubleVisionFadeIn);
					dvStrength = ApplyFadeCurve(dvTime / dvFade, curve) *
						settings->fallDoubleVisionMaxStrength;
				}

				if (currentPhase == Phase::Phase3 && settings->fallMotionBlurEnabled) {
					float mbTime = fallTime - (settings->fallPhase1Duration + settings->fallPhase2Duration);
					if (mbTime > 0.0f) {
						float mbFade = std::max(0.01f, settings->fallMotionBlurFadeIn);
						mbStrength = ApplyFadeCurve(mbTime / mbFade, curve) *
							settings->fallMotionBlurMaxStrength;
					}
				}
			} else if (currentPhase == Phase::Landing) {
				float t = Clamp01(landingTime / std::max(0.1f, settings->fallAudioFadeOut * 0.5f));
				float k = 1.0f - SmoothStep(t);
				dvStrength = lastDoubleVision * k;
				mbStrength = lastMotionBlur   * k;
			}

			if (!settings->fallDoubleVisionEnabled) dvStrength = 0.0f;
			if (!settings->fallMotionBlurEnabled)   mbStrength = 0.0f;

			ApplyIMODStrengths(dvStrength, mbStrength);
		}
	}
}
