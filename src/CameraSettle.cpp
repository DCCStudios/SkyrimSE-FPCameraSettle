#include "CameraSettle.h"
#include "Settings.h"

namespace CameraSettle
{
	namespace
	{
		constexpr float PI = 3.14159265358979323846f;
		constexpr float DEG_TO_RAD = PI / 180.0f;
		constexpr float RAD_TO_DEG = 180.0f / PI;
		
		RE::NiPoint3 ClampVector(const RE::NiPoint3& a_vec, float a_max)
		{
			return {
				std::clamp(a_vec.x, -a_max, a_max),
				std::clamp(a_vec.y, -a_max, a_max),
				std::clamp(a_vec.z, -a_max, a_max)
			};
		}
		
		RE::NiPoint3 LerpVector(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float a_t)
		{
			return {
				a_from.x + (a_to.x - a_from.x) * a_t,
				a_from.y + (a_to.y - a_from.y) * a_t,
				a_from.z + (a_to.z - a_from.z) * a_t
			};
		}
		
		// Create rotation matrix from euler angles (pitch, yaw, roll order)
		RE::NiMatrix3 EulerToMatrix(float a_pitch, float a_yaw, float a_roll)
		{
			float cx = std::cos(a_pitch);
			float sx = std::sin(a_pitch);
			float cy = std::cos(a_yaw);
			float sy = std::sin(a_yaw);
			float cz = std::cos(a_roll);
			float sz = std::sin(a_roll);
			
			RE::NiMatrix3 result;
			result.entry[0][0] = cy * cz;
			result.entry[0][1] = -cy * sz;
			result.entry[0][2] = sy;
			result.entry[1][0] = sx * sy * cz + cx * sz;
			result.entry[1][1] = -sx * sy * sz + cx * cz;
			result.entry[1][2] = -sx * cy;
			result.entry[2][0] = -cx * sy * cz + sx * sz;
			result.entry[2][1] = cx * sy * sz + sx * cz;
			result.entry[2][2] = cx * cy;
			
			return result;
		}
	}
	
	void CameraSettleManager::ApplyImpulse(SpringState& a_state, PendingBlend& a_blend, const ActionSettings& a_settings, float a_multiplier, Settings* a_globalSettings)
	{
		// Early out before any string operations
		if (!a_settings.enabled || a_multiplier <= 0.0f || a_settings.multiplier <= 0.0f) {
			// Only do debug work if debug is actually enabled
			if (a_globalSettings->debugLogging) {
				logger::info("[FPCameraSettle] ApplyImpulse: BLOCKED (enabled={}, globalMult={:.2f}, actionMult={:.2f})",
					a_settings.enabled, a_multiplier, a_settings.multiplier);
			}
			if (a_globalSettings->debugOnScreen) {
				char buf[128];
				snprintf(buf, sizeof(buf), "FPCam: BLOCKED mult=%.1fx%.1f", a_multiplier, a_settings.multiplier);
				RE::DebugNotification(buf);
			}
			return;
		}
		
		if (a_globalSettings->debugLogging) {
			logger::info("[FPCameraSettle] ApplyImpulse: enabled={}, globalMult={:.2f}, actionMult={:.2f}, posStr={:.2f}",
				a_settings.enabled, a_multiplier, a_settings.multiplier, a_settings.positionStrength);
		}
		
		// Include per-action multiplier (0-10x range)
		float totalMult = a_multiplier * a_settings.multiplier;
		float posMult = a_settings.positionStrength * totalMult;
		float rotMult = a_settings.rotationStrength * DEG_TO_RAD * totalMult;
		
		// Calculate target impulse
		RE::NiPoint3 posImpulse = {
			a_settings.impulseX * posMult,
			a_settings.impulseY * posMult,
			a_settings.impulseZ * posMult
		};
		
		RE::NiPoint3 rotImpulse = {
			a_settings.rotImpulseX * rotMult,
			a_settings.rotImpulseY * rotMult,
			a_settings.rotImpulseZ * rotMult
		};
		
		// If blend time is 0 or very small, apply instantly
		if (a_settings.blendTime < 0.001f) {
			a_state.positionVelocity.x += posImpulse.x;
			a_state.positionVelocity.y += posImpulse.y;
			a_state.positionVelocity.z += posImpulse.z;
			a_state.rotationVelocity.x += rotImpulse.x;
			a_state.rotationVelocity.y += rotImpulse.y;
			a_state.rotationVelocity.z += rotImpulse.z;
			
			if (a_globalSettings->debugLogging) {
				logger::info("[FPCameraSettle] Impulse applied instantly: posVel=({:.2f},{:.2f},{:.2f}) totalMult={:.2f}",
					a_state.positionVelocity.x, a_state.positionVelocity.y, a_state.positionVelocity.z, totalMult);
			}
			if (a_globalSettings->debugOnScreen) {
				char buf[128];
				snprintf(buf, sizeof(buf), "FPCam: impulse %.1fx%.1f=%.1f", a_multiplier, a_settings.multiplier, totalMult);
				RE::DebugNotification(buf);
			}
		} else {
			// Start a blend - add to any existing blend
			if (a_blend.active) {
				// Add remaining impulse from previous blend instantly
				float remaining = 1.0f - a_blend.progress;
				a_state.positionVelocity.x += a_blend.posImpulse.x * remaining;
				a_state.positionVelocity.y += a_blend.posImpulse.y * remaining;
				a_state.positionVelocity.z += a_blend.posImpulse.z * remaining;
				a_state.rotationVelocity.x += a_blend.rotImpulse.x * remaining;
				a_state.rotationVelocity.y += a_blend.rotImpulse.y * remaining;
				a_state.rotationVelocity.z += a_blend.rotImpulse.z * remaining;
			}
			
			// Set up new blend
			a_blend.active = true;
			a_blend.progress = 0.0f;
			a_blend.duration = a_settings.blendTime;
			a_blend.multiplier = totalMult;
			a_blend.posImpulse = posImpulse;
			a_blend.rotImpulse = rotImpulse;
			
			if (a_globalSettings->debugLogging) {
				logger::info("[FPCameraSettle] Impulse blend started: duration={:.2f}s target=({:.2f},{:.2f},{:.2f}) totalMult={:.2f}",
					a_settings.blendTime, posImpulse.x, posImpulse.y, posImpulse.z, totalMult);
			}
			if (a_globalSettings->debugOnScreen) {
				char buf[128];
				snprintf(buf, sizeof(buf), "FPCam: blend %.1fx%.1f=%.1f (%.2fs)", a_multiplier, a_settings.multiplier, totalMult, a_settings.blendTime);
				RE::DebugNotification(buf);
			}
		}
	}
	
	void CameraSettleManager::UpdateBlend(SpringState& a_state, PendingBlend& a_blend, float a_delta)
	{
		if (!a_blend.active || a_delta <= 0.0f) {
			return;
		}
		
		// Calculate how much progress this frame
		float prevProgress = a_blend.progress;
		a_blend.progress += a_delta / a_blend.duration;
		
		if (a_blend.progress >= 1.0f) {
			// Blend complete - apply remaining impulse
			float remaining = 1.0f - prevProgress;
			a_state.positionVelocity.x += a_blend.posImpulse.x * remaining;
			a_state.positionVelocity.y += a_blend.posImpulse.y * remaining;
			a_state.positionVelocity.z += a_blend.posImpulse.z * remaining;
			a_state.rotationVelocity.x += a_blend.rotImpulse.x * remaining;
			a_state.rotationVelocity.y += a_blend.rotImpulse.y * remaining;
			a_state.rotationVelocity.z += a_blend.rotImpulse.z * remaining;
			
			a_blend.Reset();
		} else {
			// Apply this frame's portion of the impulse
			float deltaProgress = a_blend.progress - prevProgress;
			a_state.positionVelocity.x += a_blend.posImpulse.x * deltaProgress;
			a_state.positionVelocity.y += a_blend.posImpulse.y * deltaProgress;
			a_state.positionVelocity.z += a_blend.posImpulse.z * deltaProgress;
			a_state.rotationVelocity.x += a_blend.rotImpulse.x * deltaProgress;
			a_state.rotationVelocity.y += a_blend.rotImpulse.y * deltaProgress;
			a_state.rotationVelocity.z += a_blend.rotImpulse.z * deltaProgress;
		}
	}
	
	void CameraSettleManager::UpdateSpring(SpringState& a_state, const ActionSettings& a_settings, float a_delta, Settings* a_globalSettings)
	{
		if (a_delta <= 0.0f) {
			return;
		}
		
		// Spring parameters
		float k = a_settings.stiffness;
		float c = a_settings.damping;
		
		// Apply settling - increase damping when idle
		float dampingMult = 1.0f + (settlingFactor * (a_globalSettings->settleDampingMult - 1.0f));
		c *= dampingMult;
		
		float m = 1.0f;
		
		// Sub-stepping for stability (configurable via settings)
		constexpr float MAX_SUBSTEP = 0.016f;
		int numSteps = static_cast<int>(std::ceil(a_delta / MAX_SUBSTEP));
		numSteps = std::clamp(numSteps, 1, a_globalSettings->springSubsteps);
		float stepDelta = a_delta / static_cast<float>(numSteps);
		
		constexpr float MAX_POS_VELOCITY = 200.0f;
		constexpr float MAX_ROT_VELOCITY = 20.0f;
		
		for (int step = 0; step < numSteps; ++step) {
			// Position spring: F = -k * position - c * velocity (target is 0,0,0)
			RE::NiPoint3 posForce = {
				-k * a_state.positionOffset.x - c * a_state.positionVelocity.x,
				-k * a_state.positionOffset.y - c * a_state.positionVelocity.y,
				-k * a_state.positionOffset.z - c * a_state.positionVelocity.z
			};
			
			a_state.positionVelocity.x += (posForce.x / m) * stepDelta;
			a_state.positionVelocity.y += (posForce.y / m) * stepDelta;
			a_state.positionVelocity.z += (posForce.z / m) * stepDelta;
			
			a_state.positionVelocity = ClampVector(a_state.positionVelocity, MAX_POS_VELOCITY);
			
			a_state.positionOffset.x += a_state.positionVelocity.x * stepDelta;
			a_state.positionOffset.y += a_state.positionVelocity.y * stepDelta;
			a_state.positionOffset.z += a_state.positionVelocity.z * stepDelta;
			
			a_state.positionOffset = ClampVector(a_state.positionOffset, a_settings.positionStrength * 3.0f);
			
			// Rotation spring
			RE::NiPoint3 rotForce = {
				-k * a_state.rotationOffset.x - c * a_state.rotationVelocity.x,
				-k * a_state.rotationOffset.y - c * a_state.rotationVelocity.y,
				-k * a_state.rotationOffset.z - c * a_state.rotationVelocity.z
			};
			
			a_state.rotationVelocity.x += (rotForce.x / m) * stepDelta;
			a_state.rotationVelocity.y += (rotForce.y / m) * stepDelta;
			a_state.rotationVelocity.z += (rotForce.z / m) * stepDelta;
			
			a_state.rotationVelocity = ClampVector(a_state.rotationVelocity, MAX_ROT_VELOCITY);
			
			a_state.rotationOffset.x += a_state.rotationVelocity.x * stepDelta;
			a_state.rotationOffset.y += a_state.rotationVelocity.y * stepDelta;
			a_state.rotationOffset.z += a_state.rotationVelocity.z * stepDelta;
			
			float maxRotRad = a_settings.rotationStrength * DEG_TO_RAD * 3.0f;
			a_state.rotationOffset = ClampVector(a_state.rotationOffset, maxRotRad);
		}
	}
	
	ActionType CameraSettleManager::DetectMovementAction(RE::PlayerCharacter* a_player)
	{
		auto* playerControls = RE::PlayerControls::GetSingleton();
		if (!playerControls) {
			return ActionType::kTotal;
		}
		
		RE::NiPoint2 inputVec = playerControls->data.moveInputVec;
		
		// Threshold for movement detection
		constexpr float THRESHOLD = 0.3f;
		
		bool movingForward = inputVec.y > THRESHOLD;
		bool movingBackward = inputVec.y < -THRESHOLD;
		bool movingLeft = inputVec.x < -THRESHOLD;
		bool movingRight = inputVec.x > THRESHOLD;
		
		bool isSprinting = a_player->AsActorState()->IsSprinting();
		// IsWalking() returns true if the walk/run toggle is set to walk
		// If not walking, player runs when moving
		bool isWalking = a_player->AsActorState()->IsWalking();
		
		// Determine action based on movement
		if (isSprinting && movingForward) {
			return ActionType::SprintForward;
		}
		
		// If walking flag is not set, player runs
		if (!isWalking) {
			if (movingForward) return ActionType::RunForward;
			if (movingBackward) return ActionType::RunBackward;
			if (movingLeft) return ActionType::RunLeft;
			if (movingRight) return ActionType::RunRight;
		} else {
			if (movingForward) return ActionType::WalkForward;
			if (movingBackward) return ActionType::WalkBackward;
			if (movingLeft) return ActionType::WalkLeft;
			if (movingRight) return ActionType::WalkRight;
		}
		
		return ActionType::kTotal;  // No movement
	}
	
	void CameraSettleManager::DetectActions(RE::PlayerCharacter* a_player, float a_delta)
	{
		// Use cached settings pointer (passed from Update)
		auto* settings = Settings::GetSingleton();
		
		auto* actorState = a_player->AsActorState();
		if (!actorState) return;
		
		bool weaponDrawn = actorState->IsWeaponDrawn();
		bool isSprinting = actorState->IsSprinting();
		bool isSneaking = actorState->IsSneaking();
		bool isInAir = a_player->IsInMidair();
		
		// Determine which settings to use
		bool useDrawnSettings = weaponDrawn && settings->weaponDrawnEnabled;
		bool useSheathedSettings = !weaponDrawn && settings->weaponSheathedEnabled;
		
		float stateMult = weaponDrawn ? settings->weaponDrawnMult : settings->weaponSheathedMult;
		float globalMult = settings->globalIntensity * stateMult;
		
		if (!useDrawnSettings && !useSheathedSettings) {
			return;
		}
		
		// Update cooldowns
		if (landingCooldown > 0.0f) landingCooldown -= a_delta;
		if (hitCooldown > 0.0f) hitCooldown -= a_delta;
		if (movementDebounce > 0.0f) movementDebounce -= a_delta;
		
		// === SNEAK DETECTION ===
		if (isSneaking && !wasSneaking) {
			const auto& sneakSettings = settings->GetActionSettingsForState(ActionType::Sneak, weaponDrawn);
			ApplyImpulse(sneakSpring, sneakBlend, sneakSettings, globalMult, settings);
			timeSinceAction = 0.0f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Sneak");
		} else if (!isSneaking && wasSneaking) {
			const auto& unSneakSettings = settings->GetActionSettingsForState(ActionType::UnSneak, weaponDrawn);
			ApplyImpulse(sneakSpring, sneakBlend, unSneakSettings, globalMult, settings);
			timeSinceAction = 0.0f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: UnSneak");
		}
		wasSneaking = isSneaking;
		
		// === JUMP/LAND DETECTION ===
		if (isInAir) {
			airTime += a_delta;
		}
		
		if (isInAir && !wasInAir) {
			const auto& jumpSettings = settings->GetActionSettingsForState(ActionType::Jump, weaponDrawn);
			ApplyImpulse(jumpSpring, jumpBlend, jumpSettings, globalMult, settings);
			timeSinceAction = 0.0f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Jump");
		} else if (!isInAir && wasInAir && landingCooldown <= 0.0f) {
			const auto& landSettings = settings->GetActionSettingsForState(ActionType::Land, weaponDrawn);
			float airTimeMult = std::clamp(0.3f + airTime * 0.7f, 0.3f, 2.0f);
			ApplyImpulse(jumpSpring, jumpBlend, landSettings, globalMult * airTimeMult, settings);
			timeSinceAction = 0.0f;
			landingCooldown = 0.25f;
			airTime = 0.0f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Land (airTime={:.2f})", airTime);
		}
		wasInAir = isInAir;
		
		// === SPRINT DETECTION ===
		if (isSprinting && !wasSprinting) {
			const auto& sprintSettings = settings->GetActionSettingsForState(ActionType::SprintForward, weaponDrawn);
			ApplyImpulse(movementSpring, movementBlend, sprintSettings, globalMult, settings);
			timeSinceAction = 0.0f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Sprint Start");
		} else if (!isSprinting && wasSprinting) {
			if (!sprintStopTriggeredByAnim) {
				const auto& sprintSettings = settings->GetActionSettingsForState(ActionType::SprintForward, weaponDrawn);
				ActionSettings reverseSettings = sprintSettings;
				reverseSettings.impulseY = -reverseSettings.impulseY * 0.7f;
				reverseSettings.rotImpulseX = -reverseSettings.rotImpulseX * 0.7f;
				ApplyImpulse(movementSpring, movementBlend, reverseSettings, globalMult, settings);
				timeSinceAction = 0.0f;
				if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Sprint Stop (state fallback)");
			}
			sprintStopTriggeredByAnim = false;
		}
		wasSprinting = isSprinting;
		
		// === MOVEMENT DETECTION (walk/run start/stop) ===
		ActionType currentMovement = DetectMovementAction(a_player);
		bool isMoving = (currentMovement != ActionType::kTotal);
		
		// OPTIMIZATION: Only check walk state when moving (avoid unnecessary function call)
		bool isWalking = isMoving ? actorState->IsWalking() : wasWalking;
		
		// OPTIMIZATION: Only update blend when moving
		if (isMoving) {
			constexpr float WALK_RUN_BLEND_SPEED = 5.0f;
			float targetBlend = isWalking ? 0.0f : 1.0f;
			
			if (walkRunBlend < targetBlend) {
				walkRunBlend = std::min(walkRunBlend + WALK_RUN_BLEND_SPEED * a_delta, targetBlend);
			} else if (walkRunBlend > targetBlend) {
				walkRunBlend = std::max(walkRunBlend - WALK_RUN_BLEND_SPEED * a_delta, targetBlend);
			}
		}
		
		// OPTIMIZATION: Cache blended settings - only recalculate when blend, weapon state, or settings change
		// Only check settings version when edit mode is enabled (for performance)
		uint32_t currentSettingsVersion = settings->GetVersion();
		bool settingsChanged = settings->IsEditMode() && (currentSettingsVersion != lastSettingsVersion);
		bool needsBlendRecalc = (std::abs(walkRunBlend - lastWalkRunBlend) > 0.01f) || 
		                        (weaponDrawn != lastBlendWeaponDrawn) ||
		                        settingsChanged;
		if (needsBlendRecalc) {
			// Update cached blended settings for all 4 directions
			const auto& walkFwd = settings->GetActionSettingsForState(ActionType::WalkForward, weaponDrawn);
			const auto& runFwd = settings->GetActionSettingsForState(ActionType::RunForward, weaponDrawn);
			cachedBlendedWalkRun[0] = ActionSettings::Blend(walkFwd, runFwd, walkRunBlend);
			
			const auto& walkBack = settings->GetActionSettingsForState(ActionType::WalkBackward, weaponDrawn);
			const auto& runBack = settings->GetActionSettingsForState(ActionType::RunBackward, weaponDrawn);
			cachedBlendedWalkRun[1] = ActionSettings::Blend(walkBack, runBack, walkRunBlend);
			
			const auto& walkLeft = settings->GetActionSettingsForState(ActionType::WalkLeft, weaponDrawn);
			const auto& runLeft = settings->GetActionSettingsForState(ActionType::RunLeft, weaponDrawn);
			cachedBlendedWalkRun[2] = ActionSettings::Blend(walkLeft, runLeft, walkRunBlend);
			
			const auto& walkRight = settings->GetActionSettingsForState(ActionType::WalkRight, weaponDrawn);
			const auto& runRight = settings->GetActionSettingsForState(ActionType::RunRight, weaponDrawn);
			cachedBlendedWalkRun[3] = ActionSettings::Blend(walkRight, runRight, walkRunBlend);
			
			lastWalkRunBlend = walkRunBlend;
			lastBlendWeaponDrawn = weaponDrawn;
			lastSettingsVersion = currentSettingsVersion;
		}
		
		// Helper to get cached blended movement settings (no allocation)
		auto getCachedBlendedSettings = [&](ActionType moveType) -> const ActionSettings& {
			switch (moveType) {
				case ActionType::WalkForward:
				case ActionType::RunForward:
					return cachedBlendedWalkRun[0];
				case ActionType::WalkBackward:
				case ActionType::RunBackward:
					return cachedBlendedWalkRun[1];
				case ActionType::WalkLeft:
				case ActionType::RunLeft:
					return cachedBlendedWalkRun[2];
				case ActionType::WalkRight:
				case ActionType::RunRight:
					return cachedBlendedWalkRun[3];
				default:
					return settings->GetActionSettingsForState(moveType, weaponDrawn);
			}
		};
		
		// Detect walk/run state change while moving
		if (isMoving && wasMoving && wasWalking != isWalking && movementDebounce <= 0.0f) {
			const ActionSettings& blendedSettings = getCachedBlendedSettings(currentMovement);
			ApplyImpulse(movementSpring, movementBlend, blendedSettings, globalMult * 0.3f, settings);
			timeSinceAction = 0.0f;
			movementDebounce = 0.1f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Walk/Run Transition (blend={:.2f}, weapon={})", walkRunBlend, weaponDrawn ? "drawn" : "sheathed");
		}
		wasWalking = isWalking;
		
		// Detect movement start
		if (isMoving && !wasMoving && movementDebounce <= 0.0f) {
			const ActionSettings& moveSettings = getCachedBlendedSettings(currentMovement);
			ApplyImpulse(movementSpring, movementBlend, moveSettings, globalMult, settings);
			timeSinceAction = 0.0f;
			movementDebounce = 0.15f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: {} Start (blend={:.2f}, weapon={})", Settings::GetActionName(currentMovement), walkRunBlend, weaponDrawn ? "drawn" : "sheathed");
			if (settings->debugOnScreen) {
				char buf[128];
				snprintf(buf, sizeof(buf), "FPCam: %s [%s] mult=%.2f", 
					Settings::GetActionName(currentMovement),
					weaponDrawn ? "DRAWN" : "SHEATH",
					globalMult);
				RE::DebugNotification(buf);
			}
		}
		// Detect movement stop
		else if (!isMoving && wasMoving && movementDebounce <= 0.0f) {
			if (lastMovementAction != ActionType::kTotal) {
				const ActionSettings& moveSettings = getCachedBlendedSettings(lastMovementAction);
				ActionSettings stopSettings = moveSettings;
				stopSettings.impulseX = -stopSettings.impulseX * 0.5f;
				stopSettings.impulseY = -stopSettings.impulseY * 0.5f;
				stopSettings.impulseZ = -stopSettings.impulseZ * 0.3f;
				stopSettings.rotImpulseX = -stopSettings.rotImpulseX * 0.5f;
				stopSettings.rotImpulseY = -stopSettings.rotImpulseY * 0.5f;
				stopSettings.rotImpulseZ = -stopSettings.rotImpulseZ * 0.5f;
				ApplyImpulse(movementSpring, movementBlend, stopSettings, globalMult, settings);
			}
			timeSinceAction = 0.0f;
			movementDebounce = 0.15f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Movement Stop");
		}
		// Detect movement direction change
		else if (isMoving && currentMovement != currentMovementAction && currentMovementAction != ActionType::kTotal && movementDebounce <= 0.0f) {
			const ActionSettings& moveSettings = getCachedBlendedSettings(currentMovement);
			ApplyImpulse(movementSpring, movementBlend, moveSettings, globalMult * 0.5f, settings);
			timeSinceAction = 0.0f;
			movementDebounce = 0.1f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Direction Change to {} (weapon={})", Settings::GetActionName(currentMovement), weaponDrawn ? "drawn" : "sheathed");
			if (settings->debugOnScreen) {
				char buf[128];
				snprintf(buf, sizeof(buf), "FPCam: -> %s [%s]", 
					Settings::GetActionName(currentMovement),
					weaponDrawn ? "DRAWN" : "SHEATH");
				RE::DebugNotification(buf);
			}
		}
		
		wasMoving = isMoving;
		if (isMoving) {
			lastMovementAction = currentMovement;
		}
		currentMovementAction = currentMovement;
		wasWeaponDrawn = weaponDrawn;
	}
	
	RE::BSEventNotifyControl CameraSettleManager::ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*)
	{
		if (!a_event || !isInFirstPerson || hitCooldown > 0.0f) {
			return RE::BSEventNotifyControl::kContinue;
		}
		
		auto* settings = Settings::GetSingleton();
		if (!settings->enabled) {
			return RE::BSEventNotifyControl::kContinue;
		}
		
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return RE::BSEventNotifyControl::kContinue;
		}
		
		bool weaponDrawn = player->AsActorState()->IsWeaponDrawn();
		float stateMult = weaponDrawn ? settings->weaponDrawnMult : settings->weaponSheathedMult;
		float globalMult = settings->globalIntensity * stateMult;
		
		// Check if player is involved
		bool playerHit = (a_event->target.get() == player);
		bool playerHitting = (a_event->cause.get() == player);
		
		if (playerHit) {
			const auto& hitSettings = settings->GetActionSettingsForState(ActionType::TakingHit, weaponDrawn);
			ApplyImpulse(hitSpring, hitBlend, hitSettings, globalMult, settings);
			hitCooldown = 0.1f;
			timeSinceAction = 0.0f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Taking Hit");
		} else if (playerHitting) {
			const auto& hittingSettings = settings->GetActionSettingsForState(ActionType::Hitting, weaponDrawn);
			ApplyImpulse(hitSpring, hitBlend, hittingSettings, globalMult, settings);
			hitCooldown = 0.05f;
			timeSinceAction = 0.0f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Hitting");
		}
		
		return RE::BSEventNotifyControl::kContinue;
	}
	
	RE::BSEventNotifyControl CameraSettleManager::ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*)
	{
		if (!a_event || !isInFirstPerson) {
			return RE::BSEventNotifyControl::kContinue;
		}
		
		// Only process events from the player
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || a_event->holder != player) {
			return RE::BSEventNotifyControl::kContinue;
		}
		
		auto* settings = Settings::GetSingleton();
		if (!settings->enabled) {
			return RE::BSEventNotifyControl::kContinue;
		}
		
		bool weaponDrawn = player->AsActorState()->IsWeaponDrawn();
		float stateMult = weaponDrawn ? settings->weaponDrawnMult : settings->weaponSheathedMult;
		float globalMult = settings->globalIntensity * stateMult;
		
		// Check for arrow release event
		if (a_event->tag == "arrowRelease" || a_event->tag == "BoltRelease") {
			const auto& arrowSettings = settings->GetActionSettingsForState(ActionType::ArrowRelease, weaponDrawn);
			ApplyImpulse(archerySpring, archeryBlend, arrowSettings, globalMult, settings);
			timeSinceAction = 0.0f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Arrow/Bolt Release (anim event)");
		}
		// Check for sprint stop animation event (EndAnimatedCameraDelta)
		else if (a_event->tag == "EndAnimatedCameraDelta") {
			if (wasSprinting) {
				const auto& sprintSettings = settings->GetActionSettingsForState(ActionType::SprintForward, weaponDrawn);
				ActionSettings reverseSettings = sprintSettings;
				reverseSettings.impulseY = -reverseSettings.impulseY * 0.7f;
				reverseSettings.rotImpulseX = -reverseSettings.rotImpulseX * 0.7f;
				ApplyImpulse(movementSpring, movementBlend, reverseSettings, globalMult, settings);
				timeSinceAction = 0.0f;
				sprintStopTriggeredByAnim = true;
				if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Sprint Stop (anim event)");
			}
		}
		
		return RE::BSEventNotifyControl::kContinue;
	}
	
	void CameraSettleManager::TriggerAction(ActionType a_action)
	{
		auto* settings = Settings::GetSingleton();
		if (!settings->enabled || !isInFirstPerson) {
			return;
		}
		
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) return;
		
		bool weaponDrawn = player->AsActorState()->IsWeaponDrawn();
		float stateMult = weaponDrawn ? settings->weaponDrawnMult : settings->weaponSheathedMult;
		float globalMult = settings->globalIntensity * stateMult;
		
		const auto& actionSettings = settings->GetActionSettingsForState(a_action, weaponDrawn);
		
		// Route to appropriate spring
		switch (a_action) {
		case ActionType::Jump:
		case ActionType::Land:
			ApplyImpulse(jumpSpring, jumpBlend, actionSettings, globalMult, settings);
			break;
		case ActionType::Sneak:
		case ActionType::UnSneak:
			ApplyImpulse(sneakSpring, sneakBlend, actionSettings, globalMult, settings);
			break;
		case ActionType::TakingHit:
		case ActionType::Hitting:
			ApplyImpulse(hitSpring, hitBlend, actionSettings, globalMult, settings);
			break;
		case ActionType::ArrowRelease:
			ApplyImpulse(archerySpring, archeryBlend, actionSettings, globalMult, settings);
			break;
		default:
			ApplyImpulse(movementSpring, movementBlend, actionSettings, globalMult, settings);
			break;
		}
		
		timeSinceAction = 0.0f;
	}
	
	void CameraSettleManager::Update(float a_delta)
	{
		auto* settings = Settings::GetSingleton();
		if (!settings->enabled) {
			return;
		}
		
		// Handle game pause state
		auto* ui = RE::UI::GetSingleton();
		bool isGamePaused = ui && (ui->GameIsPaused() || ui->numPausesGame > 0);
		
		if (isGamePaused) {
			// Reset springs when transitioning to paused state (if enabled)
			if (!wasGamePaused && settings->resetOnPause) {
				Reset();
				if (settings->debugLogging) {
					logger::info("[FPCameraSettle] Game paused - springs reset");
				}
			}
			wasGamePaused = true;
			return;
		}
		wasGamePaused = false;
		
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		
		// Register for animation events on the player (once)
		if (!animEventRegistered) {
			player->AddAnimationGraphEventSink(this);
			animEventRegistered = true;
			logger::info("[FPCameraSettle] Registered for player animation events");
		}
		
		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!camera) {
			return;
		}
		
		// Only process in first person
		if (!camera->IsInFirstPerson()) {
			if (isInFirstPerson) {
				Reset();
				isInFirstPerson = false;
			}
			return;
		}
		
		if (!isInFirstPerson) {
			isInFirstPerson = true;
			Reset();
			logger::info("[FPCameraSettle] Entered first person");
		}
		
		debugFrameCounter++;
		
		// Detect actions and apply impulses
		DetectActions(player, a_delta);
		
		// Update settling factor
		timeSinceAction += a_delta;
		if (timeSinceAction > settings->settleDelay) {
			float settleTime = timeSinceAction - settings->settleDelay;
			settlingFactor = std::min(1.0f, settleTime * settings->settleSpeed);
		} else {
			settlingFactor = 0.0f;
		}
		
		// Get current weapon state for settings lookup
		bool weaponDrawn = player->AsActorState()->IsWeaponDrawn();
		
		// Update all springs with their respective settings
		// Use a common settings template for spring updates
		ActionSettings commonSettings;
		commonSettings.stiffness = 100.0f;
		commonSettings.damping = 8.0f;
		commonSettings.positionStrength = 5.0f;
		commonSettings.rotationStrength = 3.0f;
		
		// Use specific settings for each spring category
		// Update pending blends (applies impulses smoothly over time)
		UpdateBlend(movementSpring, movementBlend, a_delta);
		UpdateBlend(jumpSpring, jumpBlend, a_delta);
		UpdateBlend(sneakSpring, sneakBlend, a_delta);
		UpdateBlend(hitSpring, hitBlend, a_delta);
		UpdateBlend(archerySpring, archeryBlend, a_delta);
		
		// Update spring physics (pass settings pointer to avoid repeated singleton lookups)
		if (currentMovementAction != ActionType::kTotal) {
			const auto& moveSettings = settings->GetActionSettingsForState(currentMovementAction, weaponDrawn);
			UpdateSpring(movementSpring, moveSettings, a_delta, settings);
		} else {
			UpdateSpring(movementSpring, commonSettings, a_delta, settings);
		}
		
		UpdateSpring(jumpSpring, settings->GetActionSettingsForState(ActionType::Jump, weaponDrawn), a_delta, settings);
		UpdateSpring(sneakSpring, settings->GetActionSettingsForState(ActionType::Sneak, weaponDrawn), a_delta, settings);
		UpdateSpring(hitSpring, settings->GetActionSettingsForState(ActionType::TakingHit, weaponDrawn), a_delta, settings);
		UpdateSpring(archerySpring, settings->GetActionSettingsForState(ActionType::ArrowRelease, weaponDrawn), a_delta, settings);
		
		// === UPDATE IDLE CAMERA NOISE ===
		{
			// Check if we're truly idle:
			// - Not moving (walking/running/sprinting)
			// - Not in air (jumping/falling)
			// - Not sneaking
			// - Not swimming
			// - No active springs from other actions (movement, jump, sneak, hit, archery)
			auto* playerState = player->AsActorState();
			
			bool hasActiveActions = movementSpring.IsActive() || jumpSpring.IsActive() || 
			                        sneakSpring.IsActive() || hitSpring.IsActive() || archerySpring.IsActive();
			bool hasPendingBlends = movementBlend.active || jumpBlend.active || 
			                        sneakBlend.active || hitBlend.active || archeryBlend.active;
			
			bool isGrounded = !wasInAir && !player->IsInMidair();
			bool isStandingStill = !wasMoving && !playerState->IsSprinting();
			bool isNotInAction = !playerState->IsSneaking() && !playerState->IsSwimming() && 
			                     !hasActiveActions && !hasPendingBlends;
			
			bool isIdle = isGrounded && isStandingStill && isNotInAction;
			
			// Get appropriate idle noise settings based on weapon state
			bool noiseEnabled = weaponDrawn ? settings->idleNoiseEnabledDrawn : settings->idleNoiseEnabledSheathed;
			
			// Blend speed for smooth transitions (same rate for fade in and fade out)
			constexpr float IDLE_BLEND_SPEED = 3.0f;  // Lower = slower/smoother transition
			float blendFactor = std::min(1.0f, IDLE_BLEND_SPEED * a_delta);
			
			// Calculate target noise values
			RE::NiPoint3 targetPosNoise = { 0.0f, 0.0f, 0.0f };
			RE::NiPoint3 targetRotNoise = { 0.0f, 0.0f, 0.0f };
			
			if (isIdle && noiseEnabled) {
				float freq = weaponDrawn ? settings->idleNoiseFrequencyDrawn : settings->idleNoiseFrequencySheathed;
				
				// Advance noise time
				idleNoiseTime += a_delta * freq * 2.0f * PI;
				
				// Use multiple sine waves with different phases for natural-looking noise
				float sin1 = std::sin(idleNoiseTime);
				float sin2 = std::sin(idleNoiseTime * 1.37f + 1.2f);  // Slightly different frequency
				float sin3 = std::sin(idleNoiseTime * 0.73f + 2.5f);  // Even slower frequency
				
				// Calculate target position noise
				float posX = weaponDrawn ? settings->idleNoisePosAmpXDrawn : settings->idleNoisePosAmpXSheathed;
				float posY = weaponDrawn ? settings->idleNoisePosAmpYDrawn : settings->idleNoisePosAmpYSheathed;
				float posZ = weaponDrawn ? settings->idleNoisePosAmpZDrawn : settings->idleNoisePosAmpZSheathed;
				
				targetPosNoise.x = sin1 * posX;
				targetPosNoise.y = sin2 * posY;
				targetPosNoise.z = sin3 * posZ;  // Breathing effect
				
				// Calculate target rotation noise (convert degrees to radians)
				float rotX = weaponDrawn ? settings->idleNoiseRotAmpXDrawn : settings->idleNoiseRotAmpXSheathed;
				float rotY = weaponDrawn ? settings->idleNoiseRotAmpYDrawn : settings->idleNoiseRotAmpYSheathed;
				float rotZ = weaponDrawn ? settings->idleNoiseRotAmpZDrawn : settings->idleNoiseRotAmpZSheathed;
				
				targetRotNoise.x = sin3 * rotX * DEG_TO_RAD;  // Slow breathing pitch
				targetRotNoise.y = sin1 * rotY * DEG_TO_RAD;  // Roll
				targetRotNoise.z = sin2 * rotZ * DEG_TO_RAD;  // Slight yaw
			}
			// else: targetPosNoise and targetRotNoise remain at 0, causing fade out
			
			// Smoothly blend current noise toward target (works for both fade in and fade out)
			idleNoiseOffset = LerpVector(idleNoiseOffset, targetPosNoise, blendFactor);
			idleNoiseRotation = LerpVector(idleNoiseRotation, targetRotNoise, blendFactor);
		}
		
		// === UPDATE SPRINT EFFECTS (FOV + BLUR) ===
		{
			// Use sprint state, but also check if the EndAnimatedCameraDelta event fired
			// which signals the sprint stop animation has started (blend out early)
			bool isSprinting = wasSprinting && !sprintStopTriggeredByAnim;
			
			// Calculate target FOV offset
			float targetFovOffset = 0.0f;
			if (settings->sprintFovEnabled && isSprinting) {
				targetFovOffset = settings->sprintFovDelta;
			}
			
			// Smoothly blend FOV offset
			float fovBlendFactor = 1.0f - std::pow(1.0f - std::min(settings->sprintFovBlendSpeed * a_delta, 0.99f), 1.0f);
			currentFovOffset = currentFovOffset + (targetFovOffset - currentFovOffset) * fovBlendFactor;
			
			// Calculate target blur strength
			float targetBlurStrength = 0.0f;
			if (settings->sprintBlurEnabled && isSprinting) {
				targetBlurStrength = settings->sprintBlurStrength;
			}
			
			// Smoothly blend blur strength
			float blurBlendFactor = 1.0f - std::pow(1.0f - std::min(settings->sprintBlurBlendSpeed * a_delta, 0.99f), 1.0f);
			currentBlurStrength = currentBlurStrength + (targetBlurStrength - currentBlurStrength) * blurBlendFactor;
			
			// Apply blur using IMOD radial blur (copied from GetHit IMOD)
			if (sprintImod && sprintImod->radialBlur.strength) {
				// Update radial blur strength and timing parameters
				sprintImod->radialBlur.strength->floatValue = currentBlurStrength;
				
				// Update ramp timings from user settings
				if (sprintImod->radialBlur.rampUp) {
					sprintImod->radialBlur.rampUp->floatValue = settings->sprintBlurRampUp;
				}
				if (sprintImod->radialBlur.rampDown) {
					sprintImod->radialBlur.rampDown->floatValue = settings->sprintBlurRampDown;
				}
				// Update blur start radius (center clarity)
				// Higher value = blur starts further from center, keeping center clear
				if (sprintImod->radialBlur.start) {
					sprintImod->radialBlur.start->floatValue = settings->sprintBlurRadius;
				}
				
				if (currentBlurStrength > 0.01f) {
					if (!blurEffectActive) {
						// Trigger the IMOD
						sprintImodInstance = RE::ImageSpaceModifierInstanceForm::Trigger(sprintImod, 1.0f, nullptr);
						blurEffectActive = true;
						if (settings->debugLogging) {
							logger::info("[FPCameraSettle] Sprint radial blur activated (strength: {:.2f}, rampUp: {:.2f}s, rampDown: {:.2f}s)", 
								currentBlurStrength, settings->sprintBlurRampUp, settings->sprintBlurRampDown);
						}
					}
				} else if (blurEffectActive) {
					// Stop the IMOD
					RE::ImageSpaceModifierInstanceForm::Stop(sprintImod);
					sprintImodInstance = nullptr;
					blurEffectActive = false;
					if (settings->debugLogging) {
						logger::info("[FPCameraSettle] Sprint radial blur deactivated");
					}
				}
			}
		}
		
		// Debug logging
		if (settings->debugLogging && debugFrameCounter % 60 == 0) {
			bool anyActive = movementSpring.IsActive() || jumpSpring.IsActive() || 
			                 sneakSpring.IsActive() || hitSpring.IsActive() || archerySpring.IsActive();
			if (anyActive) {
				RE::NiPoint3 totalPos = {
					movementSpring.positionOffset.x + jumpSpring.positionOffset.x + sneakSpring.positionOffset.x + hitSpring.positionOffset.x + archerySpring.positionOffset.x,
					movementSpring.positionOffset.y + jumpSpring.positionOffset.y + sneakSpring.positionOffset.y + hitSpring.positionOffset.y + archerySpring.positionOffset.y,
					movementSpring.positionOffset.z + jumpSpring.positionOffset.z + sneakSpring.positionOffset.z + hitSpring.positionOffset.z + archerySpring.positionOffset.z
				};
				logger::info("[FPCameraSettle] Total offset: pos=({:.2f},{:.2f},{:.2f}) settling={:.2f}",
					totalPos.x, totalPos.y, totalPos.z, settlingFactor);
			}
		}
	}
	
	void CameraSettleManager::ApplyCameraOffset(RE::PlayerCamera* a_camera)
	{
		if (!a_camera || !isInFirstPerson) {
			return;
		}
		
		// Skip applying offsets when game is paused (if resetOnPause is enabled)
		auto* settings = Settings::GetSingleton();
		if (settings->resetOnPause) {
			auto* ui = RE::UI::GetSingleton();
			if (ui && (ui->GameIsPaused() || ui->numPausesGame > 0)) {
				return;
			}
		}
		
		// Combine all spring offsets + idle noise
		RE::NiPoint3 totalPosOffset = {
			movementSpring.positionOffset.x + jumpSpring.positionOffset.x + sneakSpring.positionOffset.x + hitSpring.positionOffset.x + archerySpring.positionOffset.x + idleNoiseOffset.x,
			movementSpring.positionOffset.y + jumpSpring.positionOffset.y + sneakSpring.positionOffset.y + hitSpring.positionOffset.y + archerySpring.positionOffset.y + idleNoiseOffset.y,
			movementSpring.positionOffset.z + jumpSpring.positionOffset.z + sneakSpring.positionOffset.z + hitSpring.positionOffset.z + archerySpring.positionOffset.z + idleNoiseOffset.z
		};
		
		RE::NiPoint3 totalRotOffset = {
			movementSpring.rotationOffset.x + jumpSpring.rotationOffset.x + sneakSpring.rotationOffset.x + hitSpring.rotationOffset.x + archerySpring.rotationOffset.x + idleNoiseRotation.x,
			movementSpring.rotationOffset.y + jumpSpring.rotationOffset.y + sneakSpring.rotationOffset.y + hitSpring.rotationOffset.y + archerySpring.rotationOffset.y + idleNoiseRotation.y,
			movementSpring.rotationOffset.z + jumpSpring.rotationOffset.z + sneakSpring.rotationOffset.z + hitSpring.rotationOffset.z + archerySpring.rotationOffset.z + idleNoiseRotation.z
		};
		
		// OPTIMIZATION: Use squared magnitudes to avoid sqrt
		constexpr float MIN_POS_SQ = 0.001f * 0.001f;  // 0.000001
		constexpr float MIN_ROT_SQ = 0.0001f * 0.0001f;  // 0.00000001
		
		float posMagSq = totalPosOffset.x * totalPosOffset.x + totalPosOffset.y * totalPosOffset.y + totalPosOffset.z * totalPosOffset.z;
		float rotMagSq = totalRotOffset.x * totalRotOffset.x + totalRotOffset.y * totalRotOffset.y + totalRotOffset.z * totalRotOffset.z;
		
		if (posMagSq < MIN_POS_SQ && rotMagSq < MIN_ROT_SQ) {
			return;
		}
		
		// Get camera node - same pattern as ImprovedCameraSE
		auto* cameraNode = a_camera->cameraRoot.get();
		if (!cameraNode) {
			return;
		}
		
		// OPTIMIZATION: Cache NiCamera pointer - only look up when camera node changes
		if (cachedCameraNode != cameraNode) {
			cachedCameraNode = cameraNode;
			cachedNiCamera = nullptr;
			
			auto* cameraNodeAsNode = cameraNode->AsNode();
			if (cameraNodeAsNode) {
				auto& children = cameraNodeAsNode->GetChildren();
				if (children.size() > 0 && children[0]) {
					cachedNiCamera = skyrim_cast<RE::NiCamera*>(children[0].get());
				}
			}
		}
		
		RE::NiCamera* cameraNI = cachedNiCamera;
		
		if (!cameraNI) {
			// Fallback: just modify cameraNode
			cameraNode->local.translate.x += totalPosOffset.x;
			cameraNode->local.translate.y += totalPosOffset.y;
			cameraNode->local.translate.z += totalPosOffset.z;
			
			if (rotMagSq > MIN_ROT_SQ) {
				RE::NiMatrix3 rotMatrix = EulerToMatrix(totalRotOffset.x, totalRotOffset.y, totalRotOffset.z);
				cameraNode->local.rotate = cameraNode->local.rotate * rotMatrix;
			}
			return;
		}
		
		// Apply position offset to both cameraNode and cameraNI (like ImprovedCameraSE)
		// This is the key - we need to update the world.translate of the NiCamera directly
		cameraNode->local.translate.x += totalPosOffset.x;
		cameraNode->local.translate.y += totalPosOffset.y;
		cameraNode->local.translate.z += totalPosOffset.z;
		
		// Also update world transforms directly (ImprovedCameraSE sets all three at once)
		cameraNode->world.translate = cameraNode->local.translate;
		cameraNI->world.translate = cameraNode->world.translate;
		
		// Apply rotation offset
		if (rotMagSq > MIN_ROT_SQ) {
			RE::NiMatrix3 rotMatrix = EulerToMatrix(totalRotOffset.x, totalRotOffset.y, totalRotOffset.z);
			cameraNode->local.rotate = cameraNode->local.rotate * rotMatrix;
			cameraNode->world.rotate = cameraNode->local.rotate;
			cameraNI->world.rotate = cameraNode->world.rotate;
		}
		
		// Apply FOV offset (for sprint effect)
		if (std::abs(currentFovOffset) > 0.01f) {
			// Capture base FOV if not done yet
			if (!fovCaptured) {
				baseFov = a_camera->worldFOV;
				fovCaptured = true;
			}
			
			// Apply FOV change
			a_camera->worldFOV = baseFov + currentFovOffset;
		} else if (fovCaptured && std::abs(currentFovOffset) <= 0.01f) {
			// Reset to base FOV when effect is off
			a_camera->worldFOV = baseFov;
		}
		
		// Update node with dirty flag (like ImprovedCameraSE's Helper::UpdateNode)
		RE::NiUpdateData updateData;
		updateData.flags = RE::NiUpdateData::Flag::kDirty;
		cameraNI->Update(updateData);
	}
	
	void CameraSettleManager::Reset()
	{
		movementSpring.Reset();
		jumpSpring.Reset();
		sneakSpring.Reset();
		hitSpring.Reset();
		archerySpring.Reset();
		
		// Reset pending blends
		movementBlend.Reset();
		jumpBlend.Reset();
		sneakBlend.Reset();
		hitBlend.Reset();
		archeryBlend.Reset();
		
		currentMovementAction = ActionType::kTotal;
		lastMovementAction = ActionType::kTotal;
		wasWeaponDrawn = false;
		wasSprinting = false;
		wasSneaking = false;
		wasInAir = false;
		wasMoving = false;
		walkRunBlend = 0.0f;
		wasWalking = true;
		airTime = 0.0f;
		landingCooldown = 0.0f;
		movementDebounce = 0.0f;
		settlingFactor = 0.0f;
		timeSinceAction = 0.0f;
		hitCooldown = 0.0f;
		debugFrameCounter = 0;
		sprintStopTriggeredByAnim = false;
		
		// Reset performance caches
		cachedNiCamera = nullptr;
		cachedCameraNode = nullptr;
		lastWalkRunBlend = -1.0f;  // Force recalculation
		lastBlendWeaponDrawn = false;
		hotReloadTimer = 0.0f;
		
		// Reset idle noise state
		idleNoiseTime = 0.0f;
		idleNoiseOffset = { 0.0f, 0.0f, 0.0f };
		idleNoiseRotation = { 0.0f, 0.0f, 0.0f };
		
		// Reset sprint effects state
		currentFovOffset = 0.0f;
		currentBlurStrength = 0.0f;
		fovCaptured = false;
		
		// Stop blur effect if active
		if (blurEffectActive && sprintImod) {
			RE::ImageSpaceModifierInstanceForm::Stop(sprintImod);
			sprintImodInstance = nullptr;
			blurEffectActive = false;
		}
		// Note: Don't destroy sprintImod here - it persists
		
		// Don't reset animEventRegistered - it persists across resets
		
		logger::info("[FPCameraSettle] Springs reset");
	}

	namespace Hook
	{
		// Main update hook - runs physics calculations
		class MainUpdateHook
		{
		public:
			static void Install()
			{
				auto& trampoline = SKSE::GetTrampoline();
				
				REL::Relocation<std::uintptr_t> hook{ RELOCATION_ID(35565, 36564) };
				
				logger::info("[FPCameraSettle] Main update hook address: {:X}", hook.address());
				
				_originalUpdate = trampoline.write_call<5>(hook.address() + RELOCATION_OFFSET(0x748, 0xC26), OnUpdate);
				
				logger::info("[FPCameraSettle] Main update hook installed");
			}

		private:
			static void OnUpdate()
			{
				_originalUpdate();
				
				// Use real wall-clock time for smooth physics (not game time which has timescale)
				static auto lastTime = std::chrono::steady_clock::now();
				auto now = std::chrono::steady_clock::now();
				float delta = std::chrono::duration<float>(now - lastTime).count();
				lastTime = now;
				
				// Clamp delta to reasonable range
				delta = std::clamp(delta, 0.001f, 0.1f);
				
				// OPTIMIZATION: Hot reload timer check moved here (avoid function call overhead)
				auto* manager = CameraSettleManager::GetSingleton();
				auto* settings = Settings::GetSingleton();
				
				manager->hotReloadTimer += delta;
				if (settings->enableHotReload && manager->hotReloadTimer >= settings->hotReloadIntervalSec) {
					manager->hotReloadTimer = 0.0f;
					settings->CheckForReload(delta);
				}
				
				// Update camera settle physics
				manager->Update(delta);
			}

			static inline REL::Relocation<decltype(OnUpdate)> _originalUpdate;
		};
		
		// Camera update hook - applies offsets after game's camera update
		class CameraUpdateHook
		{
		public:
			static void Install()
			{
				auto& trampoline = SKSE::GetTrampoline();
				
				// Hook the camera update function
				REL::Relocation<std::uintptr_t> hook{ RELOCATION_ID(49852, 50784) };
				
				logger::info("[FPCameraSettle] Camera update hook address: {:X}", hook.address());
				
				_originalCameraUpdate = trampoline.write_call<5>(hook.address() + 0x1A6, OnCameraUpdate);
				
				logger::info("[FPCameraSettle] Camera update hook installed");
			}

		private:
			static void OnCameraUpdate(RE::TESCamera* a_camera)
			{
				_originalCameraUpdate(a_camera);
				
				// Apply our offsets after the game's camera update
				auto* playerCamera = RE::PlayerCamera::GetSingleton();
				if (playerCamera && playerCamera == a_camera) {
					CameraSettleManager::GetSingleton()->ApplyCameraOffset(playerCamera);
				}
			}

			static inline REL::Relocation<decltype(OnCameraUpdate)> _originalCameraUpdate;
		};
	}
	
	// Initialize the IMOD for sprint blur effect
	// Uses the GetHit IMOD (0x162) which has proper radial blur setup
	void InitializeSprintBlurIMOD()
	{
		auto* manager = CameraSettleManager::GetSingleton();
		
		// Find the GetHit IMOD (FormID 0x162) which has radial blur configured
		RE::TESImageSpaceModifier* sourceImod = nullptr;
		
		auto* form = RE::TESForm::LookupByID(0x162);
		if (form) {
			sourceImod = form->As<RE::TESImageSpaceModifier>();
			if (sourceImod && sourceImod->radialBlur.strength) {
				logger::info("[FPCameraSettle] Found GetHit IMOD (FormID 0x162) with radial blur");
			} else {
				logger::warn("[FPCameraSettle] GetHit IMOD found but radialBlur.strength is null");
				sourceImod = nullptr;
			}
		}
		
		// Fallback: search all IMODs for one with radial blur
		if (!sourceImod) {
			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			if (dataHandler) {
				for (auto* imod : dataHandler->GetFormArray<RE::TESImageSpaceModifier>()) {
					if (imod && imod->radialBlur.strength) {
						sourceImod = imod;
						const char* editorID = imod->GetFormEditorID();
						logger::info("[FPCameraSettle] Found source radial blur IMOD: {} (FormID: {:X})", 
							editorID ? editorID : "unknown", imod->GetFormID());
						break;
					}
				}
			}
		}
		
		if (!sourceImod) {
			logger::error("[FPCameraSettle] No source IMOD with radial blur found - blur effect disabled");
			manager->sprintImod = nullptr;
			return;
		}
		
		// Create a new IMOD using factory
		const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESImageSpaceModifier>();
		if (!factory) {
			logger::error("[FPCameraSettle] Failed to get IMOD factory");
			manager->sprintImod = nullptr;
			return;
		}
		
		manager->sprintImod = factory->Create();
		if (!manager->sprintImod) {
			logger::error("[FPCameraSettle] Failed to create sprint blur IMOD");
			return;
		}
		
		// Copy ALL data from source IMOD
		manager->sprintImod->formFlags            = sourceImod->formFlags;
		manager->sprintImod->formType             = sourceImod->formType;
		manager->sprintImod->bloom                = sourceImod->bloom;
		manager->sprintImod->cinematic            = sourceImod->cinematic;
		manager->sprintImod->hdr                  = sourceImod->hdr;
		manager->sprintImod->radialBlur           = sourceImod->radialBlur;
		manager->sprintImod->dof                  = sourceImod->dof;
		manager->sprintImod->doubleVisionStrength = sourceImod->doubleVisionStrength;
		manager->sprintImod->fadeColor            = sourceImod->fadeColor;
		manager->sprintImod->tintColor            = sourceImod->tintColor;
		
		manager->sprintImod->SetFormEditorID("FPCameraSettleSprintBlur");
		
		// Add to data handler
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (dataHandler) {
			dataHandler->GetFormArray<RE::TESImageSpaceModifier>().push_back(manager->sprintImod);
		}
		
		logger::info("[FPCameraSettle] Sprint blur IMOD created successfully (using radial blur from GetHit)");
	}

	void Install()
	{
		// Allocate trampoline space
		SKSE::GetTrampoline().create(64);
		
		// Install hooks
		Hook::MainUpdateHook::Install();
		Hook::CameraUpdateHook::Install();
		
		// Initialize sprint blur IMOD
		InitializeSprintBlurIMOD();
		
		// Register for hit events (explicit cast needed since we inherit from multiple event sinks)
		auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
		if (eventSource) {
			eventSource->AddEventSink<RE::TESHitEvent>(CameraSettleManager::GetSingleton());
			logger::info("[FPCameraSettle] Registered for hit events");
		}
		
		logger::info("[FPCameraSettle] Camera settle system installed");
	}
}

