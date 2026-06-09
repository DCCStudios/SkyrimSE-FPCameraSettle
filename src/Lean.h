#pragma once

#include "Settings.h"

namespace Lean
{
	enum class LeanSource : int
	{
		None = 0,
		Manual,
		Contextual
	};

	class LeanManager
	{
	public:
		static LeanManager* GetSingleton()
		{
			static LeanManager singleton;
			return &singleton;
		}

		void Update(float a_delta);

		// Manual lean control (called from input handler)
		void SetManualLean(float a_target);
		void ClearManualLean();
		void ToggleLeanLeft();
		void ToggleLeanRight();

		float GetLeanCurrent() const { return leanCurrent; }
		float GetLeanTarget() const { return leanTarget; }
		LeanSource GetLeanSource() const { return leanSource; }
		bool IsLeaning() const { return std::abs(leanCurrent) > 0.001f; }

		void Reset();

		// Track last input device for gamepad-only detection
		void SetLastInputDevice(RE::INPUT_DEVICE a_device) { lastInputDevice = a_device; }
		bool IsLastInputGamepad() const { return lastInputDevice == RE::INPUT_DEVICE::kGamepad; }

	private:
		LeanManager() = default;
		~LeanManager() = default;
		LeanManager(const LeanManager&) = delete;
		LeanManager(LeanManager&&) = delete;
		LeanManager& operator=(const LeanManager&) = delete;
		LeanManager& operator=(LeanManager&&) = delete;

		void UpdateContextual(float a_delta);
		bool IsInRangedCombatStance(RE::PlayerCharacter* a_player) const;
		bool ShouldDisableLean(RE::PlayerCharacter* a_player) const;
		bool CastLeanRay(RE::bhkWorld* a_world, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float& a_hitFraction);

		float leanTarget{ 0.0f };
		float leanCurrent{ 0.0f };
		LeanSource leanSource{ LeanSource::None };

		bool toggledLeft{ false };
		bool toggledRight{ false };

		float contextualTarget{ 0.0f };
		float contextualRawTarget{ 0.0f };     // Unfiltered raycast result
		float contextualDisengageTimer{ 0.0f }; // Hysteresis: delay before disengaging
		mutable float contextualHoldTimer{ 0.0f };  // Generic hold timer for all ranged weapons
		mutable bool  contextualWasActive{ false };  // Was in ranged stance last frame

		RE::INPUT_DEVICE lastInputDevice{ RE::INPUT_DEVICE::kKeyboard };
	};

	void InstallProjectileHook();
}
