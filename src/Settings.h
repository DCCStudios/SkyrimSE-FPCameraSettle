#pragma once

// Action types that trigger camera settle effects
enum class ActionType : int
{
	WalkForward = 0,
	WalkBackward,
	WalkLeft,
	WalkRight,
	RunForward,
	RunBackward,
	RunLeft,
	RunRight,
	SprintForward,
	Jump,
	Land,
	Sneak,
	UnSneak,
	TakingHit,
	Hitting,
	ArrowRelease,  // Bow/crossbow shot
	kTotal
};

// Settings for a specific action type
struct ActionSettings
{
	bool  enabled{ true };           // Enable settle effect for this action
	float multiplier{ 1.0f };        // Per-action intensity multiplier (0-10x)
	float blendTime{ 0.1f };         // Time to blend impulse into spring (0 = instant, up to 1.0 sec)
	float stiffness{ 100.0f };       // Spring stiffness (higher = faster return)
	float damping{ 8.0f };           // Damping coefficient (higher = less oscillation)
	float positionStrength{ 5.0f };  // Position offset strength
	float rotationStrength{ 3.0f };  // Rotation offset strength (degrees)
	float impulseX{ 0.0f };          // Initial impulse direction X
	float impulseY{ 0.0f };          // Initial impulse direction Y (forward/back)
	float impulseZ{ 0.0f };          // Initial impulse direction Z (up/down)
	float rotImpulseX{ 0.0f };       // Initial rotation impulse (pitch)
	float rotImpulseY{ 0.0f };       // Initial rotation impulse (yaw)
	float rotImpulseZ{ 0.0f };       // Initial rotation impulse (roll)
	
	void Load(CSimpleIniA& a_ini, const char* a_section);
	void Save(CSimpleIniA& a_ini, const char* a_section) const;
	
	// Copy all values from another ActionSettings
	void CopyFrom(const ActionSettings& other);
	
	// Blend between two ActionSettings (t=0 returns a, t=1 returns b)
	static ActionSettings Blend(const ActionSettings& a, const ActionSettings& b, float t);
};

class Settings
{
public:
	static Settings* GetSingleton()
	{
		static Settings singleton;
		return &singleton;
	}

	void Load();
	void Save();
	void CheckForReload(float a_deltaTime);
	
	// Settings version - incremented when any setting changes (for cache invalidation)
	uint32_t GetVersion() const { return settingsVersion; }
	void MarkDirty() { settingsVersion++; }
	
	// Edit mode - when true, check for settings changes every frame
	bool IsEditMode() const { return editMode; }
	void SetEditMode(bool a_enabled) { editMode = a_enabled; if (a_enabled) settingsVersion++; }
	
	// Get settings for a specific action type
	ActionSettings& GetActionSettings(ActionType a_type);
	const ActionSettings& GetActionSettings(ActionType a_type) const;
	
	// Get action name for display
	static const char* GetActionName(ActionType a_type);

	// === MASTER TOGGLE ===
	bool enabled{ true };
	
	// === WEAPON DRAWN SETTINGS ===
	bool weaponDrawnEnabled{ true };     // Enable effects when weapon is drawn
	bool weaponSheathedEnabled{ true };  // Enable effects when weapon is sheathed
	float weaponDrawnMult{ 1.0f };       // Multiplier when weapon is drawn
	float weaponSheathedMult{ 0.7f };    // Multiplier when weapon is sheathed
	
	// === GENERAL SETTINGS ===
	float globalIntensity{ 1.0f };    // Global intensity multiplier
	float smoothingFactor{ 0.3f };    // Smoothing for input (0-1)
	
	// === SETTLING BEHAVIOR ===
	float settleDelay{ 0.1f };        // Delay before settling starts
	float settleSpeed{ 3.0f };        // How fast settling occurs
	float settleDampingMult{ 2.0f };  // Max damping multiplier when settled
	
	// === PERFORMANCE ===
	int springSubsteps{ 4 };      // Number of sub-steps for spring physics (1-8, higher = more stable but slower)
	
	// === BEHAVIOR ===
	bool resetOnPause{ false };   // Reset springs when game is paused (menus, console, etc.)
	
	// === IDLE CAMERA NOISE ===
	// Weapon Drawn
	bool  idleNoiseEnabledDrawn{ false };
	float idleNoisePosAmpXDrawn{ 0.0f };      // Position amplitude X (left/right)
	float idleNoisePosAmpYDrawn{ 0.0f };      // Position amplitude Y (forward/back)
	float idleNoisePosAmpZDrawn{ 0.02f };     // Position amplitude Z (up/down breathing)
	float idleNoiseRotAmpXDrawn{ 0.1f };      // Rotation amplitude pitch (degrees)
	float idleNoiseRotAmpYDrawn{ 0.0f };      // Rotation amplitude roll (degrees)
	float idleNoiseRotAmpZDrawn{ 0.05f };     // Rotation amplitude yaw (degrees)
	float idleNoiseFrequencyDrawn{ 0.3f };    // Noise frequency (cycles per second)
	
	// Weapon Sheathed
	bool  idleNoiseEnabledSheathed{ true };
	float idleNoisePosAmpXSheathed{ 0.0f };
	float idleNoisePosAmpYSheathed{ 0.0f };
	float idleNoisePosAmpZSheathed{ 0.03f };
	float idleNoiseRotAmpXSheathed{ 0.15f };
	float idleNoiseRotAmpYSheathed{ 0.0f };
	float idleNoiseRotAmpZSheathed{ 0.08f };
	float idleNoiseFrequencySheathed{ 0.25f };
	
	// === SPRINT EFFECTS ===
	bool  sprintFovEnabled{ true };
	float sprintFovDelta{ 10.0f };            // FOV increase when sprinting (degrees)
	float sprintFovBlendSpeed{ 3.0f };        // How fast to blend FOV (higher = faster)
	
	bool  sprintBlurEnabled{ false };
	float sprintBlurStrength{ 0.3f };         // Radial blur strength (0-1)
	float sprintBlurBlendSpeed{ 3.0f };       // How fast to blend blur (higher = faster)
	float sprintBlurRampUp{ 0.1f };           // IMOD ramp up time (seconds) - how fast blur fades in
	float sprintBlurRampDown{ 0.2f };         // IMOD ramp down time (seconds) - how fast blur fades out
	float sprintBlurRadius{ 0.5f };           // Blur start radius (0 = from center, 1 = edges only)
	
	// === DEBUG ===
	bool debugLogging{ false };
	bool debugOnScreen{ false };
	
	// === HOT RELOAD ===
	bool  enableHotReload{ true };
	float hotReloadIntervalSec{ 5.0f };
	
	// === PER-ACTION SETTINGS ===
	// Weapon drawn actions
	ActionSettings walkForwardDrawn;
	ActionSettings walkBackwardDrawn;
	ActionSettings walkLeftDrawn;
	ActionSettings walkRightDrawn;
	ActionSettings runForwardDrawn;
	ActionSettings runBackwardDrawn;
	ActionSettings runLeftDrawn;
	ActionSettings runRightDrawn;
	ActionSettings sprintForwardDrawn;
	ActionSettings jumpDrawn;
	ActionSettings landDrawn;
	ActionSettings sneakDrawn;
	ActionSettings unSneakDrawn;
	ActionSettings takingHitDrawn;
	ActionSettings hittingDrawn;
	ActionSettings arrowReleaseDrawn;
	
	// Weapon sheathed actions
	ActionSettings walkForwardSheathed;
	ActionSettings walkBackwardSheathed;
	ActionSettings walkLeftSheathed;
	ActionSettings walkRightSheathed;
	ActionSettings runForwardSheathed;
	ActionSettings runBackwardSheathed;
	ActionSettings runLeftSheathed;
	ActionSettings runRightSheathed;
	ActionSettings sprintForwardSheathed;
	ActionSettings jumpSheathed;
	ActionSettings landSheathed;
	ActionSettings sneakSheathed;
	ActionSettings unSneakSheathed;
	ActionSettings takingHitSheathed;
	ActionSettings hittingSheathed;
	ActionSettings arrowReleaseSheathed;  // For consistency, though unlikely to trigger
	
	// Get appropriate settings based on weapon drawn state
	ActionSettings& GetActionSettingsForState(ActionType a_type, bool a_weaponDrawn);
	const ActionSettings& GetActionSettingsForState(ActionType a_type, bool a_weaponDrawn) const;

private:
	Settings() = default;
	Settings(const Settings&) = delete;
	Settings(Settings&&) = delete;
	~Settings() = default;

	Settings& operator=(const Settings&) = delete;
	Settings& operator=(Settings&&) = delete;

	// Hot-reload tracking
	std::filesystem::file_time_type lastModifiedTime{};
	float timeSinceLastCheck{ 0.0f };
	
	// Version counter for cache invalidation
	uint32_t settingsVersion{ 0 };
	
	// Edit mode flag
	bool editMode{ false };
	
	// Internal helpers
	void InitializeDefaults();
};

