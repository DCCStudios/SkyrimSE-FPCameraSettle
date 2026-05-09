#pragma once

#include "Settings.h"

namespace FallEffect
{
	enum class Phase : int
	{
		Inactive      = 0,
		Phase1        = 1,  // Subtle shake + wind fade-in
		Phase2        = 2,  // Whine intro + double vision begins + stronger shake
		Phase3        = 3,  // Full double vision + downward bias + FOV oscillation
		Landing       = 4,  // Fade-out everything
		FatalLanding  = 5   // Mirror's Edge death slam: cut to black + impact
	};

	// Singleton manager that drives the full Mirror's-Edge style falling
	// disorientation effect (audio + visual + camera shake).
	class FallEffectManager
	{
	public:
		static FallEffectManager* GetSingleton()
		{
			static FallEffectManager singleton;
			return &singleton;
		}

		// Called every frame from CameraSettleManager::Update.
		// a_delta is real wall-clock seconds (not game time).
		void Update(float a_delta);

		// Per-frame additive offsets applied by ApplyCameraOffset.
		// Read AFTER Update has run for the current frame.
		const RE::NiPoint3& GetPositionOffset() const { return positionOffset; }
		const RE::NiPoint3& GetRotationOffset() const { return rotationOffset; }

		// Additive FOV offset in degrees (added on top of sprint/punch FOV).
		float GetFovOffset() const { return fovOffset; }

		// Hard reset (called on first-person enter/exit, level load, etc.).
		void Reset();

		// One-time initialization (clones IMOD with double vision + radial blur).
		// Call after Settings::Load() and after dataHandler is available.
		void Initialize();

		// Stop all looped audio + IMODs (called on plugin unload or first-person exit).
		void StopAllSounds();

		// Pause / resume waveOut devices (for game-pause, so audio doesn't
		// continue playing over menus / console / loading screens).
		void PauseAudio();
		void ResumeAudio();

		// Force the audio aliases to be closed and re-opened (used after the
		// "Reload Audio" menu button or when the path / boost has changed).
		void ReloadAudio();

		// Pre-load WAV files from disk into memory. Call once at init
		// (kDataLoaded) so we never hit disk during gameplay.
		void PreloadAudioFiles();

		// Whether the wind / whine WAVs were located on disk at last lookup.
		// Updated by RefreshAudioFileExistence(); read by the menu for the
		// status indicator.
		bool IsWindFilePresent() const   { return windFilePresent; }
		bool IsWhineFilePresent() const  { return whineFilePresent; }
		bool IsImpactFilePresent() const { return impactFilePresent; }
		void RefreshAudioFileExistence();

		// If the player died during an active fall phase (Phase1-Landing) and
		// the fall reached at least Phase2, immediately transition to
		// FatalLanding.  Must be called BEFORE any Reset() that would kill
		// the running effects (e.g. before the first-person exit check).
		void CheckDeathTransition();

		Phase GetPhase() const { return currentPhase; }
		float GetFallTime() const { return fallTime; }
		float GetIntensity01() const { return intensity01; }

	private:
		FallEffectManager() = default;
		~FallEffectManager() = default;
		FallEffectManager(const FallEffectManager&) = delete;
		FallEffectManager(FallEffectManager&&) = delete;
		FallEffectManager& operator=(const FallEffectManager&) = delete;
		FallEffectManager& operator=(FallEffectManager&&) = delete;

		// Detection
		bool ShouldTriggerFall(RE::PlayerCharacter* a_player, Settings* a_settings, float a_delta);
		bool IsBlockingState(RE::PlayerCharacter* a_player) const;
		bool IsPlayerFatallyLanded(RE::PlayerCharacter* a_player) const;

		// Transition into FatalLanding (shared by Landing-phase check and
		// the early CheckDeathTransition call).
		void EnterFatalLanding();

		// Audio (waveOut API — reads WAV into memory then plays via the
		// Windows audio mixer; reliable with MO2/USVFS and game audio).
		// Volumes are 0..5 (0..500%). Above 1.0 the PCM samples are pre-
		// amplified in memory and waveOutSetVolume handles the sub-gain.
		void PlayWind(float a_volume);
		void PlayWhine(float a_volume);
		void PlayImpact(float a_volume);
		void StopWind(bool a_immediate);
		void StopWhine(bool a_immediate);
		void StopImpact();
		void SetWindVolume(float a_volume);   // 0..5
		void SetWhineVolume(float a_volume);  // 0..5
		void TickAudioFade(float a_delta);    // Drive landing fade-out

		// Procedural shake (perlin-style noise + sine)
		float Noise1D(float x) const;
		void  ComputeShake(float a_delta);

		// IMOD: clones donors so DV, motion blur, and fade-to-black have
		// independent per-instance strength.
		void EnsureIMODs();
		void ApplyIMODStrengths(float a_doubleVision, float a_motionBlur);
		void ApplyFadeToBlack(float a_strength);
		void StopAllIMODs();

		// State
		Phase currentPhase{ Phase::Inactive };
		float fallTime{ 0.0f };       // Seconds since fall began
		float landingTime{ 0.0f };    // Seconds since landing began (for fade-out)
		float intensity01{ 0.0f };    // 0..1 ramped intensity envelope

		// Cached previous-frame state
		float prevZ{ 0.0f };
		float estVelZ{ 0.0f };        // Estimated downward velocity (units/sec)
		bool  hadValidPrevZ{ false };
		bool  wasInAir{ false };
		bool  wasBlocked{ false };     // Previous frame's IsBlockingState result
		float lowVelocityTime{ 0.0f }; // Time spent below velocity threshold (for paraglider exit)
		Phase peakPhase{ Phase::Inactive }; // Highest phase reached during current fall

		// Output offsets (read by camera offset hook)
		RE::NiPoint3 positionOffset{ 0.0f, 0.0f, 0.0f };
		RE::NiPoint3 rotationOffset{ 0.0f, 0.0f, 0.0f };
		float        fovOffset{ 0.0f };

		// Continuous phase counters for shake (so it doesn't snap)
		float shakePhase{ 0.0f };
		float fovPhase{ 0.0f };

		// Per-frame target volumes for audio (0..1 normalized)
		float windTargetVolume{ 0.0f };
		float whineTargetVolume{ 0.0f };
		float windCurrentVolume{ 0.0f };
		float whineCurrentVolume{ 0.0f };
		bool  windPlaying{ false };
		bool  whinePlaying{ false };

		// Audio fade state on landing
		bool  audioFading{ false };
		float fadeStartWindVol{ 0.0f };
		float fadeStartWhineVol{ 0.0f };
		float fadeProgress{ 0.0f };     // 0..1

		// IMOD state. Two clones — one drives only double-vision, the other
		// drives only radial blur — each with its own instance whose `strength`
		// member is written every frame.
		RE::TESImageSpaceModifier*           fallImodDV{ nullptr };
		RE::ImageSpaceModifierInstanceForm*  fallImodDVInstance{ nullptr };
		bool                                 dvActive{ false };

		RE::TESImageSpaceModifier*           fallImodMB{ nullptr };
		RE::ImageSpaceModifierInstanceForm*  fallImodMBInstance{ nullptr };
		bool                                 mbActive{ false };

		bool                                 imodInitialized{ false };
		float                                lastDoubleVision{ 0.0f };
		float                                lastMotionBlur{ 0.0f };

		// Fade-to-black IMOD (for fatal landing)
		RE::TESImageSpaceModifier*           fallImodFade{ nullptr };
		RE::ImageSpaceModifierInstanceForm*  fallImodFadeInstance{ nullptr };
		bool                                 fadeActive{ false };

		// Fatal landing state
		float fatalLandingTime{ 0.0f };     // Seconds since fatal impact
		float fatalWhineVolume{ 0.0f };     // Current whine volume during fatal spike+decay
		bool  fatalImpactPlayed{ false };   // Has impact sound been played this fatal landing?

		// Audio pause state (synced to game pause each frame)
		bool         audioPaused{ false };

		// Cached file existence (refreshed by RefreshAudioFileExistence)
		bool         windFilePresent{ false };
		bool         whineFilePresent{ false };
		bool         impactFilePresent{ false };
	};

	// Resolve full WAV file paths from the SKSE plugin folder.
	// Returns absolute path to "Data/SKSE/Plugins/FPCameraSettle/<filename>".
	std::wstring ResolveSoundPath(const wchar_t* a_fileName);
}
