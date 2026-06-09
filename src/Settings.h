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
	// Sneak movement actions
	SneakWalkForward,
	SneakWalkBackward,
	SneakWalkLeft,
	SneakWalkRight,
	SneakRunForward,
	SneakRunBackward,
	SneakRunLeft,
	SneakRunRight,
	// Other actions
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

// Saved custom preset snapshot (stores tunable noise parameters)
struct MovementNoiseSnapshot {
	float intensity{ 1.0f };
	float frequency{ 2.0f };
	float posAmpX{ 0.0f };
	float posAmpY{ 0.0f };
	float posAmpZ{ 0.0f };
	float rotAmpX{ 0.0f };
	float rotAmpY{ 0.0f };
	float rotAmpZ{ 0.0f };
	float verticalBias{ 0.5f };
	float secondHarmonic{ 0.2f };
	float lateralPhase{ 0.5f };
	float blendIn{ 0.4f };
	float blendOut{ 0.6f };

	void Load(CSimpleIniA& a_ini, const char* a_section);
	void Save(CSimpleIniA& a_ini, const char* a_section) const;
};

// Per-layer movement noise parameters (walk, run, sprint each get one)
struct MovementNoiseParams {
	bool  enabled{ false };
	int   preset{ 1 };             // 0=Custom, 1=Natural, 2=Subtle, 3=Cinematic, 4=Heavy
	float intensity{ 1.0f };
	float frequency{ 2.8f };
	float posAmpX{ 0.04f };
	float posAmpY{ 0.01f };
	float posAmpZ{ 0.06f };
	float rotAmpX{ 0.4f };
	float rotAmpY{ 0.3f };
	float rotAmpZ{ 0.15f };
	float verticalBias{ 0.6f };
	float secondHarmonic{ 0.3f };
	float lateralPhase{ 0.5f };
	float blendIn{ 0.3f };
	float blendOut{ 0.8f };

	MovementNoiseSnapshot customSnapshot;

	void Load(CSimpleIniA& a_ini, const char* a_section);
	void Save(CSimpleIniA& a_ini, const char* a_section) const;
	void SaveToCustom();
	void LoadFromCustom();
	bool DiffersFromSnapshot() const;
	void CopyTunablesFrom(const MovementNoiseParams& other);
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
	
	// === WALK/RUN BLENDING ===
	bool  speedBasedBlending{ true };    // Blend walk/run based on actual movement speed instead of binary toggle
	float walkToRunGracePeriod{ 0.15f }; // Skip walk impulse if player reaches run speed within this time (seconds)
	
	// === JUMP/LAND SCALING ===
	bool  scaleJumpByAirTime{ true };     // Scale jump/land impulse based on air time
	float jumpMinAirTime{ 0.15f };        // Minimum air time to trigger any landing impulse
	float jumpMaxAirTimeScale{ 2.0f };    // Maximum air time for scaling purposes
	float landBaseScale{ 0.3f };          // Base landing impulse scale (always applied)
	float landAirTimeScale{ 0.7f };       // Additional scale from air time (0 to this value)
	
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
	
	// Shared idle noise setting
	float idleNoiseBlendTime{ 0.25f };        // Blend in/out time in seconds
	bool  dialogueDisableIdleNoise{ false };  // Disable idle noise when in dialogue
	
	// Archery idle noise scaling
	bool  idleNoiseScaleDuringArchery{ true };    // Scale idle noise down while drawing bow/crossbow
	float idleNoiseArcheryScaleAmount{ 0.10f };  // Scale amount while drawing (0-1)
	bool  idleNoiseArcheryScaleBySkill{ false };  // Scale amount based on Archery skill

	// Sneak idle noise
	bool  idleNoiseEnabledSneaking{ true };       // Allow idle noise while sneaking and standing still
	float idleNoiseScaleSneaking{ 0.5f };         // Scale multiplier when sneaking (0-1)
	
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

	// === MOVEMENT CAMERA NOISE (rhythmic head bob while moving) ===
	MovementNoiseParams walkNoise;
	MovementNoiseParams runNoise;
	MovementNoiseParams sprintNoise;
	
	// Sprint-specific stop detection mode: 0=Sprint State, 1=Input Release, 2=Speed-Based
	int   sprintNoiseStopMode{ 1 };

	// === FOV PUNCH ===
	bool  fovPunchHitEnabled{ true };         // Enable FOV punch when taking a hit
	bool  fovPunchArrowEnabled{ true };       // Enable FOV punch on arrow/bolt release
	float fovPunchHitStrength{ 5.0f };        // Percent of FOV (5.0 = +/-5%)
	float fovPunchArrowStrength{ 3.0f };      // Percent of FOV (3.0 = +/-3%)
	float fovPunchDuration{ 0.25f };          // Total punch duration in seconds

	// === FALL EFFECT (Mirror's Edge style disorientation) ===
	bool  fallEffectEnabled{ false };          // Master toggle for the entire fall effect (opt-in)

	// Detection
	float fallTriggerTime{ 0.75f };            // Seconds in the air before effect can begin
	float fallTriggerVelocity{ 800.0f };       // Downward velocity threshold (units/sec) to trigger
	bool  fallRequireBothConditions{ false };  // If true, BOTH time AND velocity must be exceeded

	// Phase timing (seconds since fall began)
	float fallPhase1Duration{ 1.0f };          // Phase 1 -> Phase 2 transition time
	float fallPhase2Duration{ 2.0f };          // Phase 2 length (Phase 3 begins at p1+p2)

	// Camera shake
	bool  fallShakeEnabled{ true };
	float fallShakeIntensity{ 1.0f };          // Master shake multiplier (0-3)
	float fallShakeFadeIn{ 1.0f };             // Time from fall start to full shake intensity (seconds)
	float fallShakePosScale{ 1.0f };           // Position shake amount scale
	float fallShakeRotScale{ 1.0f };           // Rotation shake amount scale
	float fallShakeFrequency{ 8.0f };          // Base oscillation frequency (Hz)
	float fallShakeNoiseAmount{ 0.5f };        // 0 = pure sine, 1 = pure noise
	bool  fallShakeAffectPosition{ true };     // Apply shake to position too
	bool  fallShakeAffectRoll{ true };         // Allow roll component
	bool  fallShakeAffectPitch{ true };        // Allow pitch component
	bool  fallShakeAffectYaw{ true };          // Allow yaw component
	float fallShakeDownwardBias{ 0.4f };       // Downward pitch bias in Phase 3 (degrees added to noise)
	bool  fallShakeScaleByVelocity{ true };    // Increase shake intensity with fall speed

	// Audio - volumes are 0..5 (0..500%). Software volume scaling is applied
	// per-sample in the streaming callback — no waveOutSetVolume used.
	bool  fallAudioEnabled{ true };
	bool  fallWindEnabled{ true };
	bool  fallWhineEnabled{ true };
	float fallMasterVolume{ 1.0f };            // Master volume for ALL fall sounds (0-5)
	float fallWindMaxVolume{ 1.0f };           // Wind loop max volume (0-5)
	float fallWhineMaxVolume{ 0.6f };          // Whine loop max volume (0-5)
	float fallWindFadeIn{ 0.5f };              // Time to fade wind from 0 -> max (seconds)
	float fallWhineFadeIn{ 1.0f };             // Time to fade whine from 0 -> max (seconds)
	float fallAudioFadeOut{ 0.6f };            // Landing fade-out duration (seconds)
	bool  fallAudioVolumeByVelocity{ true };   // Scale audio volume by fall speed
	int   fallFadeCurve{ 0 };                  // 0=Linear, 1=Smooth, 2=EaseIn, 3=EaseOut, 4=Exponential

	// Visual: double vision
	bool  fallDoubleVisionEnabled{ true };
	float fallDoubleVisionMaxStrength{ 1.0f }; // 0-1
	float fallDoubleVisionFadeIn{ 1.0f };      // Time from Phase 2 start to full DV strength (seconds)
	// Visual: motion / radial blur
	bool  fallMotionBlurEnabled{ true };
	float fallMotionBlurMaxStrength{ 0.6f };
	float fallMotionBlurFadeIn{ 0.5f };        // Time from Phase 3 start to full blur strength (seconds)

	// FOV oscillation (Phase 3)
	bool  fallFovEnabled{ true };
	float fallFovOscAmplitude{ 3.0f };         // Degrees
	float fallFovOscFrequency{ 1.5f };         // Hz

	// Fatal landing (Mirror's Edge death slam)
	bool  fallFatalEnabled{ true };            // Enable the fatal landing effect
	float fallFatalBlackDuration{ 2.0f };      // How long to hold the black screen (seconds)
	float fallFatalFadeInTime{ 0.05f };        // How fast to go black (seconds, very fast)
	float fallFatalFadeOutTime{ 0.8f };        // How fast to fade back from black (seconds)
	float fallFatalWhineBoost{ 2.0f };         // Whine volume spike on impact (multiplier of max)
	float fallFatalWhineDecay{ 1.0f };         // How long the whine spike takes to fade out (seconds)
	float fallFatalImpactVolume{ 1.0f };       // Volume for the death impact sound (0-5)

	// === LEANING SYSTEM ===
	bool  leanEnabled{ false };
	float leanIntensity{ 1.0f };

	// Manual lean
	bool  leanManualEnabled{ true };
	int   leanManualMode{ 0 };            // 0=Hold, 1=Toggle
	int   leanLeftScancode{ 0x10 };       // Q key
	int   leanRightScancode{ 0x12 };      // E key

	// Contextual lean (raycasting)
	bool  leanContextualEnabled{ true };
	bool  leanContextualGamepadOnly{ false };
	float leanContextualDistance{ 150.0f };
	float leanContextualOffset{ 30.0f };
	float leanContextualDeadzone{ 0.1f };
	bool  leanContextualBow{ true };
	bool  leanContextualCrossbow{ true };
	bool  leanContextualMagic{ true };
	float leanContextualHoldTime{ 0.5f };  // Hold lean briefly after firing/casting ends
	bool  leanMagicUseHandOrigin{ true };  // Spawn spells from the actual hand node position

	// Camera offsets
	float leanPosAmount{ 12.0f };
	float leanRollDegrees{ 10.0f };
	float leanYawDegrees{ 3.0f };
	float leanForwardAmount{ 3.0f };

	// Blend speeds
	float leanBlendSpeed{ 6.0f };
	float leanReturnSpeed{ 8.0f };

	// First-person skeleton
	bool  leanFirstPersonEnabled{ true };
	float leanFirstPersonScale{ 1.0f };
	int   leanFirstPersonNode{ 2 };       // 0=Spine, 1=Spine1, 2=Spine2

	// Third-person body
	bool  leanThirdPersonEnabled{ true };
	float leanThirdPersonScale{ 1.0f };

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
	// Sneak movement actions (drawn)
	ActionSettings sneakWalkForwardDrawn;
	ActionSettings sneakWalkBackwardDrawn;
	ActionSettings sneakWalkLeftDrawn;
	ActionSettings sneakWalkRightDrawn;
	ActionSettings sneakRunForwardDrawn;
	ActionSettings sneakRunBackwardDrawn;
	ActionSettings sneakRunLeftDrawn;
	ActionSettings sneakRunRightDrawn;
	
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
	// Sneak movement actions (sheathed)
	ActionSettings sneakWalkForwardSheathed;
	ActionSettings sneakWalkBackwardSheathed;
	ActionSettings sneakWalkLeftSheathed;
	ActionSettings sneakWalkRightSheathed;
	ActionSettings sneakRunForwardSheathed;
	ActionSettings sneakRunBackwardSheathed;
	ActionSettings sneakRunLeftSheathed;
	ActionSettings sneakRunRightSheathed;
	// Other sheathed actions
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

