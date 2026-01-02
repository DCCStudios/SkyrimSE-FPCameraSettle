#pragma once

#include "Settings.h"

namespace CameraSettle
{
	// Spring state for tracking camera offset
	struct SpringState
	{
		// Position offset
		RE::NiPoint3 positionOffset{ 0.0f, 0.0f, 0.0f };
		RE::NiPoint3 positionVelocity{ 0.0f, 0.0f, 0.0f };
		
		// Rotation offset (euler angles in radians)
		RE::NiPoint3 rotationOffset{ 0.0f, 0.0f, 0.0f };
		RE::NiPoint3 rotationVelocity{ 0.0f, 0.0f, 0.0f };
		
		void Reset()
		{
			positionOffset = { 0.0f, 0.0f, 0.0f };
			positionVelocity = { 0.0f, 0.0f, 0.0f };
			rotationOffset = { 0.0f, 0.0f, 0.0f };
			rotationVelocity = { 0.0f, 0.0f, 0.0f };
		}
		
		bool IsActive() const
		{
			constexpr float THRESHOLD = 0.0001f;
			return std::abs(positionOffset.x) > THRESHOLD ||
			       std::abs(positionOffset.y) > THRESHOLD ||
			       std::abs(positionOffset.z) > THRESHOLD ||
			       std::abs(rotationOffset.x) > THRESHOLD ||
			       std::abs(rotationOffset.y) > THRESHOLD ||
			       std::abs(rotationOffset.z) > THRESHOLD ||
			       std::abs(positionVelocity.x) > THRESHOLD ||
			       std::abs(positionVelocity.y) > THRESHOLD ||
			       std::abs(positionVelocity.z) > THRESHOLD;
		}
	};
	
	// Pending blend state for smooth impulse application
	struct PendingBlend
	{
		bool active{ false };
		float progress{ 0.0f };      // 0 to 1
		float duration{ 0.1f };      // Blend time in seconds
		float multiplier{ 1.0f };    // Total multiplier for this impulse
		
		// Target impulse values
		RE::NiPoint3 posImpulse{ 0.0f, 0.0f, 0.0f };
		RE::NiPoint3 rotImpulse{ 0.0f, 0.0f, 0.0f };
		
		void Reset()
		{
			active = false;
			progress = 0.0f;
			duration = 0.1f;
			multiplier = 1.0f;
			posImpulse = { 0.0f, 0.0f, 0.0f };
			rotImpulse = { 0.0f, 0.0f, 0.0f };
		}
	};

	class CameraSettleManager : 
		public RE::BSTEventSink<RE::TESHitEvent>,
		public RE::BSTEventSink<RE::BSAnimationGraphEvent>
	{
	public:
		static CameraSettleManager* GetSingleton()
		{
			static CameraSettleManager singleton;
			return &singleton;
		}
		
		// Hot reload timer (public for hook access)
		float hotReloadTimer{ 0.0f };
		
		// Main update called from hook
		void Update(float a_delta);
		
		// Apply camera offset (called from camera update hook)
		void ApplyCameraOffset(RE::PlayerCamera* a_camera);
		
		// Reset all springs
		void Reset();
		
		// Event handling for hit detection
		RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>* a_eventSource) override;
		
		// Event handling for animation events (arrow release, etc.)
		RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) override;
		
		// Trigger a specific action effect
		void TriggerAction(ActionType a_action);

	private:
		CameraSettleManager() = default;
		~CameraSettleManager() = default;
		CameraSettleManager(const CameraSettleManager&) = delete;
		CameraSettleManager(CameraSettleManager&&) = delete;
		CameraSettleManager& operator=(const CameraSettleManager&) = delete;
		CameraSettleManager& operator=(CameraSettleManager&&) = delete;
		
		// Action detection
		void DetectActions(RE::PlayerCharacter* a_player, float a_delta);
		
		// Movement action detection
		ActionType DetectMovementAction(RE::PlayerCharacter* a_player);
		
		// Update spring physics (pass settings pointer to avoid repeated singleton lookup)
		void UpdateSpring(SpringState& a_state, const ActionSettings& a_settings, float a_delta, Settings* a_globalSettings);
		
		// Apply impulse to spring (starts a blend if blendTime > 0)
		void ApplyImpulse(SpringState& a_state, PendingBlend& a_blend, const ActionSettings& a_settings, float a_multiplier, Settings* a_globalSettings);
		
		// Update pending blend and apply impulse incrementally
		void UpdateBlend(SpringState& a_state, PendingBlend& a_blend, float a_delta);
		
		// Springs for different action categories (combined additively)
		SpringState movementSpring;   // Walk/run/sprint
		SpringState jumpSpring;       // Jump/land
		SpringState sneakSpring;      // Sneak/unsneak
		SpringState hitSpring;        // Taking hits and hitting
		SpringState archerySpring;    // Arrow release
		
		// Pending blends for each spring
		PendingBlend movementBlend;
		PendingBlend jumpBlend;
		PendingBlend sneakBlend;
		PendingBlend hitBlend;
		PendingBlend archeryBlend;
		
		// Active action tracking
		ActionType currentMovementAction{ ActionType::kTotal };
		ActionType lastMovementAction{ ActionType::kTotal };
		
		// State tracking
		bool isInFirstPerson{ false };
		bool wasWeaponDrawn{ false };
		bool wasGamePaused{ false };
		bool wasSprinting{ false };
		bool wasSneaking{ false };
		bool wasInAir{ false };
		bool wasMoving{ false };
		
		// Jump tracking
		float airTime{ 0.0f };
		float landingCooldown{ 0.0f };
		
		// Movement debounce (to trigger only on movement start/stop)
		float movementDebounce{ 0.0f };
		
		// Walk/Run blend factor (0.0 = pure walk, 1.0 = pure run)
		float walkRunBlend{ 0.0f };
		bool wasWalking{ true };  // Track previous walk/run state
		
		// Settling factor
		float settlingFactor{ 0.0f };
		float timeSinceAction{ 0.0f };
		
		// Hit tracking
		float hitCooldown{ 0.0f };
		
		// Frame counter for debug
		int debugFrameCounter{ 0 };
		
		// Animation event registration
		bool animEventRegistered{ false };
		
		// Sprint stop tracking (for anim event vs state fallback)
		bool sprintStopTriggeredByAnim{ false };
		
		// === PERFORMANCE CACHES ===
		// Cached NiCamera pointer (avoid RTTI cast every frame)
		RE::NiCamera* cachedNiCamera{ nullptr };
		RE::NiNode* cachedCameraNode{ nullptr };
		
		// Cached blended movement settings (avoid creating new ActionSettings every frame)
		ActionSettings cachedBlendedWalkRun[4];  // Forward, Backward, Left, Right
		float lastWalkRunBlend{ -1.0f };         // Track when to recalculate
		bool lastBlendWeaponDrawn{ false };
		uint32_t lastSettingsVersion{ 0 };       // Track settings changes for cache invalidation
		
		// === IDLE NOISE STATE ===
		float idleNoiseTime{ 0.0f };             // Accumulated time for noise generation
		RE::NiPoint3 idleNoiseOffset{ 0.0f, 0.0f, 0.0f };    // Current position noise offset
		RE::NiPoint3 idleNoiseRotation{ 0.0f, 0.0f, 0.0f };  // Current rotation noise offset
		
	public:
		// === SPRINT EFFECTS STATE (public for initialization) ===
		float currentFovOffset{ 0.0f };          // Current FOV offset (blended)
		float currentBlurStrength{ 0.0f };       // Current blur strength (blended)
		float baseFov{ 0.0f };                   // Base FOV captured when entering first person
		bool fovCaptured{ false };               // Whether we've captured the base FOV
		RE::TESImageSpaceModifier* sprintImod{ nullptr };  // Runtime IMOD for sprint blur
		RE::ImageSpaceModifierInstanceForm* sprintImodInstance{ nullptr };
		bool blurEffectActive{ false };
	};

	// Install hooks
	void Install();
}

