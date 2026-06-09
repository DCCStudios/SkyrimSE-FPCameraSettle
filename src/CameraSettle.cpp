#include "CameraSettle.h"
#include "Menu.h"
#include "Settings.h"
#include "PrecisionAPI.h"
#include "FallEffect.h"
#include "Lean.h"
#include <Windows.h>

namespace CameraSettle
{
	namespace
	{
		constexpr float PI = 3.14159265358979323846f;
		constexpr float DEG_TO_RAD = PI / 180.0f;
		constexpr float RAD_TO_DEG = 180.0f / PI;

		// Rotation matrix for a single axis-angle rotation (Rodrigues' formula)
		RE::NiMatrix3 AxisAngleMatrix(const RE::NiPoint3& a_axis, float a_angle)
		{
			float c = std::cos(a_angle);
			float s = std::sin(a_angle);
			float t = 1.0f - c;
			float x = a_axis.x, y = a_axis.y, z = a_axis.z;

			RE::NiMatrix3 result;
			result.entry[0][0] = t * x * x + c;
			result.entry[0][1] = t * x * y - s * z;
			result.entry[0][2] = t * x * z + s * y;
			result.entry[1][0] = t * x * y + s * z;
			result.entry[1][1] = t * y * y + c;
			result.entry[1][2] = t * y * z - s * x;
			result.entry[2][0] = t * x * z - s * y;
			result.entry[2][1] = t * y * z + s * x;
			result.entry[2][2] = t * z * z + c;
			return result;
		}

		RE::NiMatrix3 TransposeMatrix(const RE::NiMatrix3& m)
		{
			RE::NiMatrix3 r;
			r.entry[0][0] = m.entry[0][0]; r.entry[0][1] = m.entry[1][0]; r.entry[0][2] = m.entry[2][0];
			r.entry[1][0] = m.entry[0][1]; r.entry[1][1] = m.entry[1][1]; r.entry[1][2] = m.entry[2][1];
			r.entry[2][0] = m.entry[0][2]; r.entry[2][1] = m.entry[1][2]; r.entry[2][2] = m.entry[2][2];
			return r;
		}

		// Propagate world transforms down a bone hierarchy after modifying local transforms.
		// Skips effect containers to avoid disturbing VFX particles.
		void PropagateWorldTransforms(RE::NiAVObject* a_obj)
		{
			if (!a_obj) return;

			RE::NiUpdateData ud{};
			ud.flags = RE::NiUpdateData::Flag::kDirty;
			a_obj->UpdateWorldData(&ud);

			auto* node = a_obj->AsNode();
			if (!node) return;

			auto& children = node->GetChildren();
			for (std::uint16_t i = 0; i < children.capacity(); ++i) {
				auto& child = children[i];
				if (!child) continue;
				std::string_view name = child->name.c_str();
				bool isEffect = (name == "MagicEffectsNode") ||
				                (name.find("Particle") != std::string_view::npos) ||
				                (name.find("-Emitter") != std::string_view::npos);
				if (isEffect) continue;
				PropagateWorldTransforms(child.get());
			}
		}
		
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

		float SmoothStep(float a_t)
		{
			a_t = std::clamp(a_t, 0.0f, 1.0f);
			return a_t * a_t * (3.0f - 2.0f * a_t);
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

	void CameraSettleManager::StartFovPunch(float a_strengthPercent)
	{
		auto* settings = Settings::GetSingleton();
		if (!settings || a_strengthPercent <= 0.0f) {
			return;
		}
		
		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!camera) {
			return;
		}
		
		fovPunchActive = true;
		fovPunchTimer = 0.0f;
		fovPunchDuration = std::max(settings->fovPunchDuration, 0.05f);
		fovPunchStrength = std::clamp(a_strengthPercent / 100.0f, 0.0f, 0.5f);
		fovPunchValue = 0.0f;
	}

	void CameraSettleManager::RegisterPrecisionAPI()
	{
		if (precisionHitCallbacksRegistered) {
			return;
		}
		
		auto module = ::GetModuleHandleW(L"Precision.dll");
		if (!module) {
			return;
		}
		
		using RequestPluginAPIFn = void* (*)(PRECISION_API::InterfaceVersion);
		auto requestAPI = reinterpret_cast<RequestPluginAPIFn>(::GetProcAddress(module, "RequestPluginAPI"));
		if (!requestAPI) {
			return;
		}
		
		auto* api = static_cast<PRECISION_API::IVPrecision4*>(requestAPI(PRECISION_API::InterfaceVersion::V4));
		if (!api) {
			return;
		}
		
		auto handle = SKSE::GetPluginHandle();
		auto result = api->AddPostHitCallback(handle, [this](const PRECISION_API::PrecisionHitData& a_hitData, const RE::HitData& a_hitDataVanilla) {
			OnPrecisionHit(a_hitData, a_hitDataVanilla);
		});
		
		if (result == PRECISION_API::APIResult::OK || result == PRECISION_API::APIResult::AlreadyRegistered) {
			precisionApi = api;
			precisionHitCallbacksRegistered = true;
			logger::info("[FPCameraSettle] Precision API detected - using Precision hit callbacks");
		}
	}

	void CameraSettleManager::OnPrecisionHit(const PRECISION_API::PrecisionHitData& a_hitData, const RE::HitData& a_hitDataVanilla)
	{
		if (!isInFirstPerson || hitCooldown > 0.0f) {
			return;
		}
		
		auto* settings = Settings::GetSingleton();
		if (!settings || !settings->enabled) {
			return;
		}
		
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		
		if (a_hitData.target != player) {
			return;
		}
		
		bool weaponDrawn = player->AsActorState()->IsWeaponDrawn();
		float stateMult = weaponDrawn ? settings->weaponDrawnMult : settings->weaponSheathedMult;
		float globalMult = settings->globalIntensity * stateMult;
		
		float hitScale = a_hitDataVanilla.flags.any(RE::HitData::Flag::kBlocked) ? 0.5f : 1.0f;
		
		const auto& hitSettings = settings->GetActionSettingsForState(ActionType::TakingHit, weaponDrawn);
		ApplyImpulse(hitSpring, hitBlend, hitSettings, globalMult * hitScale, settings);
		if (settings->fovPunchHitEnabled) {
			StartFovPunch(settings->fovPunchHitStrength);
		}
		hitCooldown = 0.15f;
		timeSinceAction = 0.0f;
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
	
	// Helper function to check if two movement actions are opposite directions
	bool AreOppositeDirections(ActionType a_action1, ActionType a_action2)
	{
		// Check forward/backward opposites
		bool fwd1 = (a_action1 == ActionType::WalkForward || a_action1 == ActionType::RunForward || 
		             a_action1 == ActionType::SneakWalkForward || a_action1 == ActionType::SneakRunForward);
		bool back1 = (a_action1 == ActionType::WalkBackward || a_action1 == ActionType::RunBackward ||
		              a_action1 == ActionType::SneakWalkBackward || a_action1 == ActionType::SneakRunBackward);
		bool left1 = (a_action1 == ActionType::WalkLeft || a_action1 == ActionType::RunLeft ||
		              a_action1 == ActionType::SneakWalkLeft || a_action1 == ActionType::SneakRunLeft);
		bool right1 = (a_action1 == ActionType::WalkRight || a_action1 == ActionType::RunRight ||
		               a_action1 == ActionType::SneakWalkRight || a_action1 == ActionType::SneakRunRight);
		
		bool fwd2 = (a_action2 == ActionType::WalkForward || a_action2 == ActionType::RunForward ||
		             a_action2 == ActionType::SneakWalkForward || a_action2 == ActionType::SneakRunForward);
		bool back2 = (a_action2 == ActionType::WalkBackward || a_action2 == ActionType::RunBackward ||
		              a_action2 == ActionType::SneakWalkBackward || a_action2 == ActionType::SneakRunBackward);
		bool left2 = (a_action2 == ActionType::WalkLeft || a_action2 == ActionType::RunLeft ||
		              a_action2 == ActionType::SneakWalkLeft || a_action2 == ActionType::SneakRunLeft);
		bool right2 = (a_action2 == ActionType::WalkRight || a_action2 == ActionType::RunRight ||
		               a_action2 == ActionType::SneakWalkRight || a_action2 == ActionType::SneakRunRight);
		
		// Forward vs backward OR left vs right
		return (fwd1 && back2) || (back1 && fwd2) || (left1 && right2) || (right1 && left2);
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
		
		auto* actorState = a_player->AsActorState();
		bool isSprinting = actorState->IsSprinting();
		bool isSneaking = actorState->IsSneaking();
		// IsWalking() returns true if the walk/run toggle is set to walk
		// If not walking, player runs when moving
		bool isWalking = actorState->IsWalking();
		
		// Determine action based on movement
		if (isSprinting && movingForward) {
			return ActionType::SprintForward;
		}
		
		// Handle sneak movement
		if (isSneaking) {
			if (!isWalking) {
				// Sneak running
				if (movingForward) return ActionType::SneakRunForward;
				if (movingBackward) return ActionType::SneakRunBackward;
				if (movingLeft) return ActionType::SneakRunLeft;
				if (movingRight) return ActionType::SneakRunRight;
			} else {
				// Sneak walking
				if (movingForward) return ActionType::SneakWalkForward;
				if (movingBackward) return ActionType::SneakWalkBackward;
				if (movingLeft) return ActionType::SneakWalkLeft;
				if (movingRight) return ActionType::SneakWalkRight;
			}
		}
		
		// Normal (not sneaking) movement
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
		// Track air time for landing impulse scaling
		if (isInAir) {
			airTime += a_delta;
		}
		
		// Just left the ground - check if it was an actual jump or just walking off a ledge
		if (isInAir && !wasInAir) {
			// Check if player actually jumped using behavior graph variable
			// bAnimationDriven is true during jump animations
			bool bAnimDriven = false;
			a_player->GetGraphVariableBool("bAnimationDriven", bAnimDriven);
			
			// Also check jumping state
			bool isJumping = false;
			a_player->GetGraphVariableBool("IsJumping", isJumping);
			
			// Player jumped if either animation driven or in jumping state
			didJump = bAnimDriven || isJumping;
			jumpStartZ = a_player->GetPosition().z;
			
			// Only apply jump impulse if player actually jumped (not walking off ledge)
			if (didJump) {
				const auto& jumpSettings = settings->GetActionSettingsForState(ActionType::Jump, weaponDrawn);
				ApplyImpulse(jumpSpring, jumpBlend, jumpSettings, globalMult, settings);
				timeSinceAction = 0.0f;
				if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Jump (actual jump)");
			} else {
				if (settings->debugLogging) logger::info("[FPCameraSettle] Leaving ground (walk off ledge, no jump impulse)");
			}
		} 
		// Just landed
		else if (!isInAir && wasInAir && landingCooldown <= 0.0f) {
			// Calculate landing impulse scale based on air time
			float landingMult = 1.0f;
			
			if (settings->scaleJumpByAirTime) {
				// Only apply landing if air time exceeds minimum
				if (airTime < settings->jumpMinAirTime) {
					// Too short of a drop, skip landing impulse
					if (settings->debugLogging) logger::info("[FPCameraSettle] Short drop (airTime={:.3f}s < min={:.3f}s), skipping land impulse", 
						airTime, settings->jumpMinAirTime);
					airTime = 0.0f;
					didJump = false;
					wasInAir = isInAir;
					return;  // Skip the landing impulse entirely
				}
				
				// Calculate normalized air time (0 to 1 based on max scale time)
				float normalizedAirTime = std::clamp(
					(airTime - settings->jumpMinAirTime) / (settings->jumpMaxAirTimeScale - settings->jumpMinAirTime), 
					0.0f, 1.0f
				);
				
				// Base scale always applies, plus additional scale based on air time
				landingMult = settings->landBaseScale + normalizedAirTime * settings->landAirTimeScale;
				
				// If didn't actually jump (just fell), reduce slightly
				if (!didJump) {
					landingMult *= 0.8f;
				}
			} else {
				// Old behavior - simple scaling
				landingMult = std::clamp(0.3f + airTime * 0.7f, 0.3f, 2.0f);
			}
			
			const auto& landSettings = settings->GetActionSettingsForState(ActionType::Land, weaponDrawn);
			ApplyImpulse(jumpSpring, jumpBlend, landSettings, globalMult * landingMult, settings);
			timeSinceAction = 0.0f;
			landingCooldown = 0.25f;
			
			if (settings->debugLogging) {
				float fallDistance = jumpStartZ - a_player->GetPosition().z;
				logger::info("[FPCameraSettle] Action: Land (airTime={:.2f}s, fallDist={:.0f}, mult={:.2f}, wasJump={})", 
					airTime, fallDistance, landingMult, didJump ? "yes" : "no");
			}
			
			airTime = 0.0f;
			didJump = false;
		}
		wasInAir = isInAir;
		
		// === SPRINT DETECTION ===
		if (isSprinting && !wasSprinting) {
			const auto& sprintSettings = settings->GetActionSettingsForState(ActionType::SprintForward, weaponDrawn);
			ApplyImpulse(movementSpring, movementBlend, sprintSettings, globalMult, settings);
			timeSinceAction = 0.0f;
			idleNoiseAllowedAfterSprint = false;
			sprintStopTriggeredByAnim = false;
			sprintInputEndedEarly = false;
			sprintInputEndedTimer = 0.0f;
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
			// Prevent the direction-change detection from firing a SprintForward→RunForward
			// impulse on top of the sprint-stop impulse in the same frame.
			movementDebounce = 0.2f;
		}
		wasSprinting = isSprinting;
		
		// === MOVEMENT DETECTION (walk/run start/stop) ===
		ActionType currentMovement = DetectMovementAction(a_player);
		bool isMoving = (currentMovement != ActionType::kTotal);
		
		// OPTIMIZATION: Only check walk state when moving (avoid unnecessary function call)
		bool isWalking = isMoving ? actorState->IsWalking() : wasWalking;
		
		// Get current movement speed for speed-based blending
		auto* playerControls = RE::PlayerControls::GetSingleton();
		float inputMagnitude = 0.0f;
		if (playerControls) {
			RE::NiPoint2 inputVec = playerControls->data.moveInputVec;
			inputMagnitude = std::sqrt(inputVec.x * inputVec.x + inputVec.y * inputVec.y);
		}
		currentSpeed = inputMagnitude;
		
		// === SPEED-BASED WALK/RUN BLENDING ===
		if (isMoving) {
			constexpr float WALK_RUN_BLEND_SPEED = 5.0f;
			float targetBlend;
			
			if (settings->speedBasedBlending) {
				// Use input magnitude to determine blend
				// Walk is typically 0.3-0.5, run is 0.7-1.0
				// Blend smoothly between walk and run based on input magnitude
				constexpr float WALK_THRESHOLD = 0.4f;
				constexpr float RUN_THRESHOLD = 0.7f;
				
				if (inputMagnitude < WALK_THRESHOLD) {
					targetBlend = 0.0f;  // Walk
				} else if (inputMagnitude > RUN_THRESHOLD) {
					targetBlend = 1.0f;  // Run
				} else {
					// Blend between walk and run
					targetBlend = (inputMagnitude - WALK_THRESHOLD) / (RUN_THRESHOLD - WALK_THRESHOLD);
				}
				
				// If player has walk toggle on, cap at walk
				if (isWalking) {
					targetBlend = std::min(targetBlend, 0.3f);
				}
			} else {
				// Binary walk/run based on toggle
				targetBlend = isWalking ? 0.0f : 1.0f;
			}
			
			if (walkRunBlend < targetBlend) {
				walkRunBlend = std::min(walkRunBlend + WALK_RUN_BLEND_SPEED * a_delta, targetBlend);
			} else if (walkRunBlend > targetBlend) {
				walkRunBlend = std::max(walkRunBlend - WALK_RUN_BLEND_SPEED * a_delta, targetBlend);
			}
			
			// Update speed blend for grace period tracking
			speedBlend = walkRunBlend;
		}
		
		// === WALK-TO-RUN GRACE PERIOD ===
		// Track when movement starts, block walk impulse if player accelerates quickly to run
		if (isMoving && !wasMovingForGrace) {
			// Just started moving
			movementStartTime = 0.0f;
			walkImpulseBlocked = false;
		}
		if (isMoving) {
			movementStartTime += a_delta;
			
			// If within grace period and player is running/accelerating, block walk impulse
			if (movementStartTime < settings->walkToRunGracePeriod && walkRunBlend > 0.5f) {
				walkImpulseBlocked = true;
			}
		} else {
			walkImpulseBlocked = false;
		}
		wasMovingForGrace = isMoving;
		
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
		// Sneak movements use their own settings (no walk/run blending for sneak)
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
				// Sneak movements - use directly, no walk/run blending
				case ActionType::SneakWalkForward:
				case ActionType::SneakWalkBackward:
				case ActionType::SneakWalkLeft:
				case ActionType::SneakWalkRight:
				case ActionType::SneakRunForward:
				case ActionType::SneakRunBackward:
				case ActionType::SneakRunLeft:
				case ActionType::SneakRunRight:
					return settings->GetActionSettingsForState(moveType, weaponDrawn);
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
			// Check if walk impulse should be blocked (grace period for accelerating to run)
			// Only block if this is a walk-type movement and grace period is enabled
			bool isWalkMovement = (currentMovement == ActionType::WalkForward || 
			                       currentMovement == ActionType::WalkBackward ||
			                       currentMovement == ActionType::WalkLeft || 
			                       currentMovement == ActionType::WalkRight ||
			                       currentMovement == ActionType::SneakWalkForward ||
			                       currentMovement == ActionType::SneakWalkBackward ||
			                       currentMovement == ActionType::SneakWalkLeft ||
			                       currentMovement == ActionType::SneakWalkRight);
			
			// For walk movements with grace period, defer the impulse
			// We'll apply the run impulse instead if player accelerates quickly
			if (isWalkMovement && settings->walkToRunGracePeriod > 0.0f && settings->speedBasedBlending) {
				// Don't apply impulse yet - wait for grace period
				// The impulse will be applied after grace period if still walking
				// Or the run impulse will apply when speed increases
				if (settings->debugLogging) {
					logger::info("[FPCameraSettle] Walk start - grace period active (waiting {:.2f}s)", settings->walkToRunGracePeriod);
				}
			} else {
				// Normal case - apply impulse immediately
				const ActionSettings& moveSettings = getCachedBlendedSettings(currentMovement);
				ApplyImpulse(movementSpring, movementBlend, moveSettings, globalMult, settings);
				timeSinceAction = 0.0f;
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
			movementDebounce = 0.15f;
		}
		// Apply deferred walk impulse after grace period (if still walking)
		else if (isMoving && wasMoving && movementStartTime >= settings->walkToRunGracePeriod && 
		         movementStartTime < settings->walkToRunGracePeriod + a_delta * 2.0f && 
		         !walkImpulseBlocked && settings->speedBasedBlending && movementDebounce <= 0.0f) {
			// Grace period just ended and player is still walking - apply the walk impulse now
			if (walkRunBlend < 0.5f) {
				const ActionSettings& moveSettings = getCachedBlendedSettings(currentMovement);
				ApplyImpulse(movementSpring, movementBlend, moveSettings, globalMult, settings);
				timeSinceAction = 0.0f;
				movementDebounce = 0.1f;
				if (settings->debugLogging) logger::info("[FPCameraSettle] Action: {} Start (deferred after grace period)", Settings::GetActionName(currentMovement));
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
			// Check if this is an opposite direction change (forward<->back, left<->right)
			bool isOppositeDirection = AreOppositeDirections(currentMovement, currentMovementAction);
			
			if (isOppositeDirection) {
				// === OPPOSITE DIRECTION HANDLING ===
				// Instead of applying a full impulse (which fights with existing spring state),
				// we dampen the current spring velocity and apply a reduced impulse.
				// This creates a smooth "decelerate then accelerate" transition.
				
				// Dampen existing spring velocity to reduce fighting
				// This smoothly cancels the momentum from the previous direction
				float dampingFactor = 0.3f;  // Reduce velocity by 70%
				movementSpring.positionVelocity.x *= dampingFactor;
				movementSpring.positionVelocity.y *= dampingFactor;
				movementSpring.positionVelocity.z *= dampingFactor;
				movementSpring.rotationVelocity.x *= dampingFactor;
				movementSpring.rotationVelocity.y *= dampingFactor;
				movementSpring.rotationVelocity.z *= dampingFactor;
				
				// Cancel any pending blend from the previous direction
				if (movementBlend.active) {
					// Apply remaining blend at reduced strength instead of fighting
					float remainingProgress = 1.0f - movementBlend.progress;
					if (remainingProgress > 0.1f) {
						// Only apply a small portion to avoid fighting
						float reducedRemaining = remainingProgress * 0.2f;
						movementSpring.positionVelocity.x += movementBlend.posImpulse.x * reducedRemaining;
						movementSpring.positionVelocity.y += movementBlend.posImpulse.y * reducedRemaining;
						movementSpring.positionVelocity.z += movementBlend.posImpulse.z * reducedRemaining;
					}
					movementBlend.Reset();
				}
				
				// Apply the new direction's impulse at reduced strength
				// The dampened velocity + reduced impulse = smooth transition
				const ActionSettings& moveSettings = getCachedBlendedSettings(currentMovement);
				ApplyImpulse(movementSpring, movementBlend, moveSettings, globalMult * 0.25f, settings);
				
				// Longer debounce for opposite directions to prevent rapid oscillation
				movementDebounce = 0.15f;
				
				if (settings->debugLogging) {
					logger::info("[FPCameraSettle] Action: Opposite Direction {} -> {} (dampened)", 
						Settings::GetActionName(currentMovementAction), Settings::GetActionName(currentMovement));
				}
			} else {
				// === NORMAL DIRECTION CHANGE (e.g., forward to left) ===
				// These don't fight as much, apply normal impulse
				const ActionSettings& moveSettings = getCachedBlendedSettings(currentMovement);
				ApplyImpulse(movementSpring, movementBlend, moveSettings, globalMult * 0.5f, settings);
				movementDebounce = 0.1f;
				
				if (settings->debugLogging) {
					logger::info("[FPCameraSettle] Action: Direction Change to {} (weapon={})", 
						Settings::GetActionName(currentMovement), weaponDrawn ? "drawn" : "sheathed");
				}
				if (settings->debugOnScreen) {
					char buf[128];
					snprintf(buf, sizeof(buf), "FPCam: -> %s [%s]", 
						Settings::GetActionName(currentMovement),
						weaponDrawn ? "DRAWN" : "SHEATH");
					RE::DebugNotification(buf);
				}
			}
			
			timeSinceAction = 0.0f;
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
		
		// Filter out continuous damage effects (like poison, burning, etc.)
		// Allow: weapons, projectiles, ammo, explosions, hazards, and environmental damage
		// The cooldown system handles preventing spam from rapid-fire damage
		if (a_event->source != 0) {
			RE::TESForm* sourceForm = RE::TESForm::LookupByID(a_event->source);
			if (sourceForm) {
				auto formType = sourceForm->GetFormType();
				// Filter out enchantments and magic effects (often DoT sources)
				// Allow everything else: weapons, projectiles, ammo, spells (missile/aoe), explosions, etc.
				if (formType == RE::FormType::Enchantment || 
				    formType == RE::FormType::MagicEffect) {
					return RE::BSEventNotifyControl::kContinue;
				}
			}
		}
		// Note: Hits with no cause (environmental) are now allowed
		
		bool weaponDrawn = player->AsActorState()->IsWeaponDrawn();
		float stateMult = weaponDrawn ? settings->weaponDrawnMult : settings->weaponSheathedMult;
		float globalMult = settings->globalIntensity * stateMult;
		
		// Check if player is involved
		bool playerHit = (a_event->target.get() == player);
		bool playerHitting = (a_event->cause.get() == player);
		
		// If Precision is available, ignore hit events for player being hit to avoid near-miss false positives
		if (precisionHitCallbacksRegistered && playerHit) {
			return RE::BSEventNotifyControl::kContinue;
		}
		
		if (playerHit) {
			// Confirm this was an actual hit (not a miss) using lastHitData
			bool confirmedHit = false;
			float hitScale = 1.0f;
			if (auto* process = player->GetActorRuntimeData().currentProcess) {
				if (process->middleHigh && process->middleHigh->lastHitData) {
					auto* hitData = process->middleHigh->lastHitData;
					auto hitTargetPtr = hitData->target.get();
					auto hitAggressorPtr = hitData->aggressor.get();
					RE::Actor* hitTarget = hitTargetPtr.get();
					RE::Actor* hitAggressor = hitAggressorPtr.get();
					
					RE::Actor* causeActor = nullptr;
					if (auto* causeRef = a_event->cause.get()) {
						causeActor = causeRef->As<RE::Actor>();
					}
					
					bool targetMatches = (hitTarget == player);
					bool aggressorMatches = (!causeActor || hitAggressor == causeActor);
					bool hasAttackInfo =
						hitData->weapon != nullptr ||
						hitData->attackDataSpell != nullptr ||
						hitData->flags.any(RE::HitData::Flag::kMeleeAttack) ||
						hitData->flags.any(RE::HitData::Flag::kBash) ||
						hitData->flags.any(RE::HitData::Flag::kPowerAttack) ||
						hitData->flags.any(RE::HitData::Flag::kExplosion) ||
						a_event->source != 0 ||
						a_event->projectile != 0;
					
					confirmedHit = targetMatches && aggressorMatches && hasAttackInfo;
					bool blocked = hitData->flags.any(RE::HitData::Flag::kBlocked) ||
					               a_event->flags.any(RE::TESHitEvent::Flag::kHitBlocked);
					if (confirmedHit && blocked) {
						hitScale = 0.5f;
					}
				}
			}
			
			if (!confirmedHit) {
				return RE::BSEventNotifyControl::kContinue;
			}
			
			const auto& hitSettings = settings->GetActionSettingsForState(ActionType::TakingHit, weaponDrawn);
			ApplyImpulse(hitSpring, hitBlend, hitSettings, globalMult * hitScale, settings);
			if (settings->fovPunchHitEnabled) {
				StartFovPunch(settings->fovPunchHitStrength);
			}
			hitCooldown = 0.15f;  // Slightly longer cooldown to prevent rapid re-triggers
			timeSinceAction = 0.0f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Taking Hit (source: {:X})", 
				a_event->source);
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
			if (settings->fovPunchArrowEnabled) {
				StartFovPunch(settings->fovPunchArrowStrength);
			}
			archeryDrawActive = false;
			archeryReleaseTimer = 0.15f;
			timeSinceAction = 0.0f;
			if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Arrow/Bolt Release (anim event)");
		}
		// Check for sprint stop animation event (EndAnimatedCameraDelta)
		// Only trigger sprint stop if we were sprinting AND are no longer sprinting
		// (EndAnimatedCameraDelta can fire during sprint when the initial tilt animation ends)
		else if (a_event->tag == "EndAnimatedCameraDelta") {
			bool currentlySprinting = player->AsActorState() && player->AsActorState()->IsSprinting();
			if (wasSprinting && !currentlySprinting) {
				const auto& sprintSettings = settings->GetActionSettingsForState(ActionType::SprintForward, weaponDrawn);
				ActionSettings reverseSettings = sprintSettings;
				reverseSettings.impulseY = -reverseSettings.impulseY * 0.7f;
				reverseSettings.rotImpulseX = -reverseSettings.rotImpulseX * 0.7f;
				ApplyImpulse(movementSpring, movementBlend, reverseSettings, globalMult, settings);
				timeSinceAction = 0.0f;
				sprintStopTriggeredByAnim = true;
				idleNoiseAllowedAfterSprint = true;  // Allow idle noise to blend in now
				if (settings->debugLogging) logger::info("[FPCameraSettle] Action: Sprint Stop (anim event)");
			} else if (wasSprinting && currentlySprinting) {
				// Still sprinting - this is just the sprint start animation ending, allow idle noise
				idleNoiseAllowedAfterSprint = true;
				if (settings->debugLogging) logger::info("[FPCameraSettle] Sprint camera animation ended (still sprinting)");
			}
		}
		
		return RE::BSEventNotifyControl::kContinue;
	}
	
	RE::BSEventNotifyControl CameraSettleManager::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*)
	{
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}

		auto* settings = Settings::GetSingleton();

		if (!isInFirstPerson) {
			return RE::BSEventNotifyControl::kContinue;
		}

		auto* userEvents = RE::UserEvents::GetSingleton();
		if (!userEvents) {
			return RE::BSEventNotifyControl::kContinue;
		}

		for (auto* event = *a_event; event; event = event->next) {
			auto* button = event->AsButtonEvent();
			if (!button || !button->HasIDCode()) {
				continue;
			}

			// Track last input device for gamepad-only contextual lean detection
			if (button->IsDown()) {
				Lean::LeanManager::GetSingleton()->SetLastInputDevice(button->GetDevice());
			}

			// === SPRINT KEY HANDLING ===
			if (button->QUserEvent() == userEvents->sprint) {
				if (!button->IsDown()) {
					continue;
				}

				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player || !player->AsActorState()) {
					continue;
				}

				if (player->AsActorState()->IsSprinting()) {
					sprintInputEndedEarly = true;
					sprintInputEndedTimer = 0.0f;
				}
				continue;
			}

			// === LEAN KEY HANDLING ===
			if (settings->leanEnabled && settings->leanManualEnabled) {

				auto* leanMgr = Lean::LeanManager::GetSingleton();
				uint32_t encodedKey = button->GetIDCode();
				auto device = button->GetDevice();
				if (device == RE::INPUT_DEVICE::kMouse)
					encodedKey += 256;
				else if (device == RE::INPUT_DEVICE::kGamepad)
					encodedKey += 266;
				else if (device != RE::INPUT_DEVICE::kKeyboard)
					continue;

				bool isLeanLeft = (encodedKey == static_cast<uint32_t>(settings->leanLeftScancode));
				bool isLeanRight = (encodedKey == static_cast<uint32_t>(settings->leanRightScancode));

				if (!isLeanLeft && !isLeanRight) continue;

				if (settings->debugLogging) {
					logger::info("[Lean] Input: key={} device={} down={} pressed={} up={} isLeft={} isRight={} mode={}",
						encodedKey, static_cast<int>(device),
						button->IsDown(), button->IsPressed(), button->IsUp(),
						isLeanLeft, isLeanRight, settings->leanManualMode);
				}

				if (settings->leanManualMode == 0) {
					// Hold mode
					if (button->IsDown() || button->IsPressed()) {
						if (isLeanLeft) leanMgr->SetManualLean(-1.0f);
						else            leanMgr->SetManualLean(1.0f);
					} else if (button->IsUp()) {
						leanMgr->ClearManualLean();
					}
				} else {
					// Toggle mode
					if (button->IsDown()) {
						if (isLeanLeft)  leanMgr->ToggleLeanLeft();
						else             leanMgr->ToggleLeanRight();
					}
				}
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
		
		lastDeltaTime = a_delta;
		
		// Handle game pause state
		auto* ui = RE::UI::GetSingleton();
		bool isGamePaused = ui && (ui->GameIsPaused() || ui->numPausesGame > 0);
		
		if (isGamePaused) {
			auto* fallMgr = FallEffect::FallEffectManager::GetSingleton();
			bool inFatalLanding = (fallMgr->GetPhase() == FallEffect::Phase::FatalLanding);

			if (!wasGamePaused && settings->resetOnPause && !inFatalLanding) {
				Reset();
				if (settings->debugLogging) {
					logger::info("[FPCameraSettle] Game paused - springs reset");
				}
			}

			if (inFatalLanding) {
				fallMgr->Update(a_delta);
			} else {
				fallMgr->PauseAudio();
			}
			wasGamePaused = true;
			return;
		}
		// Resume fall-effect audio when returning from pause
		if (wasGamePaused) {
			FallEffect::FallEffectManager::GetSingleton()->ResumeAudio();
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
		
		// If the player died during an active fall, transition to FatalLanding
		// NOW, before the first-person check can trigger Reset() and kill it.
		FallEffect::FallEffectManager::GetSingleton()->CheckDeathTransition();

		// Only process in first person (but let FatalLanding play through death cam)
		if (!camera->IsInFirstPerson()) {
			auto* fallMgr = FallEffect::FallEffectManager::GetSingleton();
			bool inFatalLanding = (fallMgr->GetPhase() == FallEffect::Phase::FatalLanding);

			if (isInFirstPerson) {
				if (inFatalLanding) {
					// Don't reset FallEffect — let death slam finish.
					// Reset only the camera springs / sprint state.
					movementSpring.Reset();
					jumpSpring.Reset();
					sneakSpring.Reset();
					hitSpring.Reset();
					archerySpring.Reset();
				} else {
					Reset();
				}
				isInFirstPerson = false;
			}

			if (inFatalLanding) {
				fallMgr->Update(a_delta);
			}
			return;
		}
		
		if (!isInFirstPerson) {
			isInFirstPerson = true;
			Reset();
			logger::info("[FPCameraSettle] Entered first person");
		}
		
		if (!baseFovReady) {
			baseFov = camera->worldFOV - currentFovPunchOffset;
			baseFovReady = true;
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
		// This is truly additive: phase always advances, amplitude ramps smoothly
		// No lerping toward a target - noise is calculated directly from phase * amplitude
		{
			// Check if player is in a state where idle noise should play
			// IMPORTANT: We do NOT require springs to be inactive!
			// The noise is truly additive, so it layers on top of settling springs smoothly.
			// This prevents the "snap" that occurred when waiting for springs to finish.
			auto* playerState = player->AsActorState();
			
			bool isGrounded = !wasInAir && !player->IsInMidair();
			bool isStandingStill = !wasMoving && !playerState->IsSprinting();
			bool isSneakingNow = playerState->IsSneaking();
			bool isNotInActiveAction = (!isSneakingNow || settings->idleNoiseEnabledSneaking) && !playerState->IsSwimming();
			
			// Check if in dialogue or map menu (both should disable idle noise if setting enabled)
			bool isInDialogue = ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
			bool isInMapMenu = ui && ui->IsMenuOpen(RE::MapMenu::MENU_NAME);
			
			// Idle noise can only start after sprint if EndAnimatedCameraDelta has fired
			// Also disable if in dialogue/map and setting is enabled
			bool dialogueBlocksNoise = settings->dialogueDisableIdleNoise && (isInDialogue || isInMapMenu);
			
			// Player is "idle enough" for noise when standing still and grounded
			// Springs can still be settling - the noise is additive and will layer smoothly
			bool shouldPlayIdleNoise = isGrounded && isStandingStill && isNotInActiveAction && 
			                           idleNoiseAllowedAfterSprint && !dialogueBlocksNoise;
			bool noiseEnabled = weaponDrawn ? settings->idleNoiseEnabledDrawn : settings->idleNoiseEnabledSheathed;
			
			// Determine if player is currently drawing a bow/crossbow
			bool isArcheryDrawn = false;
			if (settings->idleNoiseScaleDuringArchery) {
				if (auto* weapon = player->GetEquippedObject(false)) {
					if (auto* weap = weapon->As<RE::TESObjectWEAP>()) {
						if (weap->IsBow() || weap->IsCrossbow()) {
							auto attackState = playerState->GetAttackState();
							switch (attackState) {
							case RE::ATTACK_STATE_ENUM::kBowDraw:
							case RE::ATTACK_STATE_ENUM::kBowAttached:
							case RE::ATTACK_STATE_ENUM::kBowDrawn:
							case RE::ATTACK_STATE_ENUM::kBowReleasing:
							case RE::ATTACK_STATE_ENUM::kBowNextAttack:
							case RE::ATTACK_STATE_ENUM::kBowFollowThrough:
								isArcheryDrawn = true;
								break;
							default:
								break;
							}
						}
					}
				}
			}
			
			if (archeryReleaseTimer > 0.0f) {
				archeryReleaseTimer = std::max(0.0f, archeryReleaseTimer - a_delta);
			}
			archeryDrawActive = isArcheryDrawn && archeryReleaseTimer <= 0.0f;
			
			// Log dialogue/map state transitions for debugging
			bool inBlockingMenu = isInDialogue || isInMapMenu;
			if (settings->debugLogging && inBlockingMenu != wasInDialogue) {
				const char* menuName = isInDialogue ? "Dialogue" : (isInMapMenu ? "Map" : "Menu");
				logger::info("[FPCameraSettle] {} menu: {} (noise {})", 
					menuName,
					inBlockingMenu ? "ENTERED" : "EXITED",
					dialogueBlocksNoise ? "blocked" : "allowed");
			}
			wasInDialogue = inBlockingMenu;
			
			// Get frequency for phase advancement
			float freq = weaponDrawn ? settings->idleNoiseFrequencyDrawn : settings->idleNoiseFrequencySheathed;
			
			// ALWAYS advance phase - the wave is always "there", just with zero amplitude when not idle
			// This ensures smooth continuity when amplitude ramps up/down
			idleNoisePhase += a_delta * freq * RE::BSTimer::QGlobalTimeMultiplier() * 2.0f * PI;
			
			// Keep phase bounded to avoid float precision issues over long play sessions
			if (idleNoisePhase > 1000.0f * PI) {
				idleNoisePhase = std::fmod(idleNoisePhase, 2.0f * PI);
			}
			
			// Smoothly ramp amplitude up/down based on idle state
			// This is the key to truly additive noise - only amplitude changes, not the wave itself
			float targetAmplitude = (shouldPlayIdleNoise && noiseEnabled) ? 1.0f : 0.0f;
			float rampSpeed = 3.0f / std::max(0.05f, settings->idleNoiseBlendTime);  // Match blend time
			
			if (idleNoiseAmplitude < targetAmplitude) {
				idleNoiseAmplitude = std::min(idleNoiseAmplitude + rampSpeed * a_delta, targetAmplitude);
			} else if (idleNoiseAmplitude > targetAmplitude) {
				idleNoiseAmplitude = std::max(idleNoiseAmplitude - rampSpeed * a_delta, targetAmplitude);
			}
			
			// Smoothly scale idle noise down while drawing a bow/crossbow
			float targetArcheryScale = 1.0f;
			if (settings->idleNoiseScaleDuringArchery && archeryDrawActive) {
				if (settings->idleNoiseArcheryScaleBySkill) {
					float archery = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kArchery);
					float skillT = std::clamp(archery / 100.0f, 0.0f, 1.0f);
					targetArcheryScale = std::clamp(1.0f - skillT, 0.0f, 1.0f);
				} else {
					targetArcheryScale = settings->idleNoiseArcheryScaleAmount;
				}
			}
			
			if (idleNoiseArcheryScale < targetArcheryScale) {
				idleNoiseArcheryScale = std::min(idleNoiseArcheryScale + rampSpeed * a_delta, targetArcheryScale);
			} else if (idleNoiseArcheryScale > targetArcheryScale) {
				idleNoiseArcheryScale = std::max(idleNoiseArcheryScale - rampSpeed * a_delta, targetArcheryScale);
			}

			// Smoothly scale idle noise down when sneaking (if enabled)
			float targetSneakScale = 1.0f;
			if (isSneakingNow && settings->idleNoiseEnabledSneaking) {
				targetSneakScale = settings->idleNoiseScaleSneaking;
			}
			if (idleNoiseSneakScale < targetSneakScale) {
				idleNoiseSneakScale = std::min(idleNoiseSneakScale + rampSpeed * a_delta, targetSneakScale);
			} else if (idleNoiseSneakScale > targetSneakScale) {
				idleNoiseSneakScale = std::max(idleNoiseSneakScale - rampSpeed * a_delta, targetSneakScale);
			}
			
			// Calculate sine waves from continuous phase
			float sin1 = std::sin(idleNoisePhase);
			float sin2 = std::sin(idleNoisePhase * 1.37f + 1.2f);
			float sin3 = std::sin(idleNoisePhase * 0.73f + 2.5f);
			
			// Get amplitude settings
			float posX = weaponDrawn ? settings->idleNoisePosAmpXDrawn : settings->idleNoisePosAmpXSheathed;
			float posY = weaponDrawn ? settings->idleNoisePosAmpYDrawn : settings->idleNoisePosAmpYSheathed;
			float posZ = weaponDrawn ? settings->idleNoisePosAmpZDrawn : settings->idleNoisePosAmpZSheathed;
			
			float rotX = weaponDrawn ? settings->idleNoiseRotAmpXDrawn : settings->idleNoiseRotAmpXSheathed;
			float rotY = weaponDrawn ? settings->idleNoiseRotAmpYDrawn : settings->idleNoiseRotAmpYSheathed;
			float rotZ = weaponDrawn ? settings->idleNoiseRotAmpZDrawn : settings->idleNoiseRotAmpZSheathed;
			
			// Calculate noise DIRECTLY - no lerping toward a target!
			// The amplitude smoothly ramps, so the noise smoothly appears/disappears
			// This is truly additive: sine_value * max_amplitude * current_amplitude_factor
			float finalAmplitude = idleNoiseAmplitude * idleNoiseArcheryScale * idleNoiseSneakScale;
			idleNoiseOffset.x = sin1 * posX * finalAmplitude;
			idleNoiseOffset.y = sin2 * posY * finalAmplitude;
			idleNoiseOffset.z = sin3 * posZ * finalAmplitude;
			
			idleNoiseRotation.x = sin3 * rotX * DEG_TO_RAD * finalAmplitude;
			idleNoiseRotation.y = sin1 * rotY * DEG_TO_RAD * finalAmplitude;
			idleNoiseRotation.z = sin2 * rotZ * DEG_TO_RAD * finalAmplitude;
		}
		
			// === TRACK PLAYER WORLD SPEED (for speed-based sprint ramp-down) ===
		if (settings->sprintNoiseStopMode == 2) {
			RE::NiPoint3 pos = player->GetPosition();
			if (hasLastPlayerPos && a_delta > 0.0f) {
				RE::NiPoint3 diff = { pos.x - lastPlayerPos.x, pos.y - lastPlayerPos.y, pos.z - lastPlayerPos.z };
				float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
				float instantSpeed = dist / a_delta;
				float smoothing = std::min(12.0f * a_delta, 0.85f);
				playerWorldSpeed += (instantSpeed - playerWorldSpeed) * smoothing;
			}
			lastPlayerPos = pos;
			hasLastPlayerPos = true;

			// Query sprint reference speed from the race's sprint movement type + SpeedMult
			float refSpeed = 0.0f;
			if (auto* race = player->GetRace()) {
				if (auto* sprintMoveType = race->baseMoveTypes[RE::TESRace::MovementTypes::kSprint]) {
					refSpeed = sprintMoveType->movementTypeData.defaultData.speeds
						[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun];
					float speedMult = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeedMult) / 100.0f;
					refSpeed *= std::max(speedMult, 0.01f);
				}
			}

			if (refSpeed > 1.0f) {
				float rawRatio = std::clamp(playerWorldSpeed / refSpeed, 0.0f, 1.0f);
				float ratioSmooth = std::min(8.0f * a_delta, 0.7f);
				sprintSpeedRatio += (rawRatio - sprintSpeedRatio) * ratioSmooth;
			} else {
				sprintSpeedRatio = 0.0f;
			}
		} else {
			hasLastPlayerPos = false;
		}

		// === UPDATE MOVEMENT CAMERA NOISE (three-layer: walk/run/sprint) ===
		{
			auto* playerState = player->AsActorState();
			bool isSprinting = playerState && playerState->IsSprinting() && !player->IsInMidair();
			bool isGrounded = !player->IsInMidair();

			// Determine target amplitudes for each layer
			float walkTarget = 0.0f;
			float runTarget = 0.0f;
			float sprintTarget = 0.0f;

			if (isSprinting && settings->sprintNoise.enabled) {
				sprintTarget = 1.0f;
				// Sprint stop mode 1 (Input Release)
				if (settings->sprintNoiseStopMode == 1 && sprintInputEndedEarly) {
					sprintTarget = 0.0f;
				}
			}
			if (wasMoving && !isSprinting && isGrounded) {
				if (wasWalking && settings->walkNoise.enabled) {
					walkTarget = 1.0f;
				}
				if (!wasWalking && settings->runNoise.enabled) {
					runTarget = 1.0f;
				}
			}

			// Ramp each amplitude independently with per-layer blend rates
			auto rampAmplitude = [a_delta](float& amp, float target, float blendIn, float blendOut) {
				if (amp < target) {
					float rate = 3.0f / std::max(0.05f, blendIn);
					amp = std::min(amp + rate * a_delta, target);
				} else if (amp > target) {
					float rate = 3.0f / std::max(0.05f, blendOut);
					amp = std::max(amp - rate * a_delta, target);
				}
			};

			rampAmplitude(walkNoiseAmplitude, walkTarget, settings->walkNoise.blendIn, settings->walkNoise.blendOut);
			rampAmplitude(runNoiseAmplitude, runTarget, settings->runNoise.blendIn, settings->runNoise.blendOut);
			rampAmplitude(sprintNoiseAmplitude, sprintTarget, settings->sprintNoise.blendIn, settings->sprintNoise.blendOut);

			// Sprint stop mode 2 (Speed-Based): clamp sprint amplitude to speed ratio
			if (settings->sprintNoiseStopMode == 2 && sprintTarget == 0.0f && sprintNoiseAmplitude > 0.0f) {
				sprintNoiseAmplitude = std::min(sprintNoiseAmplitude, sprintSpeedRatio);
			}

			// Normalize raw amplitudes so layers crossfade instead of stacking.
			// During transitions, blend-in/out timing can cause the sum to exceed 1.0.
			float rawSum = walkNoiseAmplitude + runNoiseAmplitude + sprintNoiseAmplitude;
			float normFactor = (rawSum > 1.0f) ? (1.0f / rawSum) : 1.0f;

			float wA = walkNoiseAmplitude * normFactor * settings->walkNoise.intensity;
			float rA = runNoiseAmplitude * normFactor * settings->runNoise.intensity;
			float sA = sprintNoiseAmplitude * normFactor * settings->sprintNoise.intensity;
			float totalAmp = wA + rA + sA;

			float blendedFreq = settings->sprintNoise.frequency;
			if (totalAmp > 0.001f) {
				blendedFreq = (wA * settings->walkNoise.frequency +
				               rA * settings->runNoise.frequency +
				               sA * settings->sprintNoise.frequency) / totalAmp;
			}

			// Advance shared phase
			float gtm = RE::BSTimer::QGlobalTimeMultiplier();
			movementNoisePhase += a_delta * blendedFreq * gtm * 2.0f * PI;
			if (movementNoisePhase > 1000.0f * PI) {
				movementNoisePhase = std::fmod(movementNoisePhase, 2.0f * PI);
			}

			if (totalAmp > 0.0001f) {
				float invTotal = 1.0f / totalAmp;

				// Blend all parameters by weighted amplitude
				auto blend = [&](float w, float r, float s) -> float {
					return (wA * w + rA * r + sA * s) * invTotal;
				};

				float bPosAmpX = blend(settings->walkNoise.posAmpX, settings->runNoise.posAmpX, settings->sprintNoise.posAmpX);
				float bPosAmpY = blend(settings->walkNoise.posAmpY, settings->runNoise.posAmpY, settings->sprintNoise.posAmpY);
				float bPosAmpZ = blend(settings->walkNoise.posAmpZ, settings->runNoise.posAmpZ, settings->sprintNoise.posAmpZ);
				float bRotAmpX = blend(settings->walkNoise.rotAmpX, settings->runNoise.rotAmpX, settings->sprintNoise.rotAmpX);
				float bRotAmpY = blend(settings->walkNoise.rotAmpY, settings->runNoise.rotAmpY, settings->sprintNoise.rotAmpY);
				float bRotAmpZ = blend(settings->walkNoise.rotAmpZ, settings->runNoise.rotAmpZ, settings->sprintNoise.rotAmpZ);
				float bBias    = blend(settings->walkNoise.verticalBias, settings->runNoise.verticalBias, settings->sprintNoise.verticalBias);
				float bH2      = blend(settings->walkNoise.secondHarmonic, settings->runNoise.secondHarmonic, settings->sprintNoise.secondHarmonic);
				float bLat     = blend(settings->walkNoise.lateralPhase, settings->runNoise.lateralPhase, settings->sprintNoise.lateralPhase);

				float phase = movementNoisePhase;
				float latPhase = bLat * PI;

				// Vertical bob with asymmetric bias
				float vertRaw = std::sin(phase);
				float vertShaped = vertRaw;
				if (bBias > 0.001f) {
					if (vertRaw < 0.0f) {
						vertShaped = vertRaw * (1.0f + bBias * 0.5f);
					} else {
						vertShaped = vertRaw * (1.0f - bBias * 0.3f);
					}
				}
				float vertFinal = vertShaped + std::sin(phase * 2.0f) * bH2;

				// Lateral sway
				float latFinal = std::sin(phase * 0.5f + latPhase) + std::sin(phase * 1.0f + latPhase) * bH2 * 0.5f;

				// Forward/back bob
				float fwdFinal = std::sin(phase + PI * 0.25f) + std::sin(phase * 2.0f + PI * 0.25f) * bH2 * 0.4f;

				// Rotation waveforms
				float pitchRaw = std::sin(phase + PI * 0.1f);
				float rollRaw  = std::sin(phase * 0.5f + latPhase + PI * 0.3f);
				float yawRaw   = std::sin(phase * 0.5f + PI * 0.7f);

				movementNoiseOffset.x = latFinal  * bPosAmpX * totalAmp;
				movementNoiseOffset.y = fwdFinal  * bPosAmpY * totalAmp;
				movementNoiseOffset.z = vertFinal * bPosAmpZ * totalAmp;

				movementNoiseRotation.x = pitchRaw * bRotAmpX * DEG_TO_RAD * totalAmp;
				movementNoiseRotation.y = rollRaw  * bRotAmpY * DEG_TO_RAD * totalAmp;
				movementNoiseRotation.z = yawRaw   * bRotAmpZ * DEG_TO_RAD * totalAmp;
			} else {
				movementNoiseOffset = { 0.0f, 0.0f, 0.0f };
				movementNoiseRotation = { 0.0f, 0.0f, 0.0f };
			}
		}

		// === UPDATE SPRINT EFFECTS (BLUR ONLY — FOV blending moved to ApplyCameraOffset for lower latency) ===
		{
			bool hasActiveBlur = std::abs(currentBlurStrength) > 0.001f;
			bool blurEnabled = settings->sprintBlurEnabled;
			
			if (!blurEnabled && !hasActiveBlur) {
				// Nothing to do
			} else {
				auto* playerState = player->AsActorState();
				bool isSprinting = playerState && playerState->IsSprinting() && !player->IsInMidair();

			float targetBlurStrength = 0.0f;
			if (settings->sprintBlurEnabled && isSprinting) {
				targetBlurStrength = settings->sprintBlurStrength;
			}

			// Stop mode 2 (Speed-Based): scale blur target by speed ratio during deceleration
			if (settings->sprintNoiseStopMode == 2 && !isSprinting && currentBlurStrength > 0.001f) {
				targetBlurStrength = settings->sprintBlurStrength * sprintSpeedRatio;
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
			}  // end else (sprint effects active)
		}

		// === UPDATE LEAN SYSTEM ===
		Lean::LeanManager::GetSingleton()->Update(a_delta);

		// === UPDATE FALL EFFECT (Mirror's Edge style disorientation) ===
		// Drives camera shake, audio, and visual IMODs. Outputs are read back
		// in ApplyCameraOffset and added to the totals.
		FallEffect::FallEffectManager::GetSingleton()->Update(a_delta);

		// === UPDATE FOV PUNCH ===
		if (fovPunchActive) {
			fovPunchTimer += a_delta;
			float t = fovPunchDuration > 0.0f ? (fovPunchTimer / fovPunchDuration) : 1.0f;
			
			if (t >= 1.0f) {
				fovPunchActive = false;
				fovPunchValue = 0.0f;
			} else {
				constexpr float PHASE1 = 0.4f;  // In -> overshoot
				float value = 0.0f;
				
				if (t < PHASE1) {
					float u = SmoothStep(t / PHASE1);
					value = -1.0f + (2.0f * u);  // -1 to +1
				} else {
					float u = SmoothStep((t - PHASE1) / (1.0f - PHASE1));
					value = 1.0f + (-1.0f * u);  // +1 to 0
				}
				fovPunchValue = value;
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
		
		// Pull fall-effect outputs (computed in CameraSettleManager::Update -> FallEffectManager::Update)
		auto* fallMgr = FallEffect::FallEffectManager::GetSingleton();
		const RE::NiPoint3& fallPos = fallMgr->GetPositionOffset();
		const RE::NiPoint3& fallRot = fallMgr->GetRotationOffset();

		// Combine all spring offsets + idle noise + movement noise + fall-effect shake
		RE::NiPoint3 totalPosOffset = {
			movementSpring.positionOffset.x + jumpSpring.positionOffset.x + sneakSpring.positionOffset.x + hitSpring.positionOffset.x + archerySpring.positionOffset.x + idleNoiseOffset.x + movementNoiseOffset.x + fallPos.x,
			movementSpring.positionOffset.y + jumpSpring.positionOffset.y + sneakSpring.positionOffset.y + hitSpring.positionOffset.y + archerySpring.positionOffset.y + idleNoiseOffset.y + movementNoiseOffset.y + fallPos.y,
			movementSpring.positionOffset.z + jumpSpring.positionOffset.z + sneakSpring.positionOffset.z + hitSpring.positionOffset.z + archerySpring.positionOffset.z + idleNoiseOffset.z + movementNoiseOffset.z + fallPos.z
		};
		
		RE::NiPoint3 totalRotOffset = {
			movementSpring.rotationOffset.x + jumpSpring.rotationOffset.x + sneakSpring.rotationOffset.x + hitSpring.rotationOffset.x + archerySpring.rotationOffset.x + idleNoiseRotation.x + movementNoiseRotation.x + fallRot.x,
			movementSpring.rotationOffset.y + jumpSpring.rotationOffset.y + sneakSpring.rotationOffset.y + hitSpring.rotationOffset.y + archerySpring.rotationOffset.y + idleNoiseRotation.y + movementNoiseRotation.y + fallRot.y,
			movementSpring.rotationOffset.z + jumpSpring.rotationOffset.z + sneakSpring.rotationOffset.z + hitSpring.rotationOffset.z + archerySpring.rotationOffset.z + idleNoiseRotation.z + movementNoiseRotation.z + fallRot.z
		};
		
		// === LEAN CAMERA OFFSETS ===
		// Rotation offsets are applied via post-multiply so they are already in camera-local
		// space. Position offsets must be rotated into world space because
		// cameraNode->local.translate is effectively in world coordinates.
		if (settings->leanEnabled) {
			auto* leanMgr = Lean::LeanManager::GetSingleton();
			float leanVal = leanMgr->GetLeanCurrent();
			if (std::abs(leanVal) > 0.001f && a_camera->IsInFirstPerson()) {
				float intensity = settings->leanIntensity;

				totalRotOffset.y += leanVal * settings->leanRollDegrees * DEG_TO_RAD * intensity;
				totalRotOffset.z += leanVal * settings->leanYawDegrees * DEG_TO_RAD * intensity;

				auto* camRoot = a_camera->cameraRoot.get();
				if (camRoot) {
					const auto& rot = camRoot->world.rotate;
					RE::NiPoint3 camRight   = { rot.entry[0][0], rot.entry[1][0], rot.entry[2][0] };
					RE::NiPoint3 camForward = { rot.entry[0][1], rot.entry[1][1], rot.entry[2][1] };

					float lateralAmount = leanVal * settings->leanPosAmount * intensity;
					float forwardAmount = std::abs(leanVal) * settings->leanForwardAmount * intensity;

					totalPosOffset.x += camRight.x * lateralAmount + camForward.x * forwardAmount;
					totalPosOffset.y += camRight.y * lateralAmount + camForward.y * forwardAmount;
					totalPosOffset.z += camRight.z * lateralAmount + camForward.z * forwardAmount;
				}
			}
		}
		
		// Blend sprint FOV offset in the camera hook for lowest latency.
		// Combines IsSprinting() with early input detection to bridge the behavior graph delay.
		{
			auto* sprintSettings = Settings::GetSingleton();
			float targetFovOffset = 0.0f;
			if (sprintSettings->sprintFovEnabled) {
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (player) {
					auto* playerState = player->AsActorState();
					bool sprintingNow = playerState && playerState->IsSprinting() && !player->IsInMidair();

					if (sprintInputEndedEarly) {
						if (!sprintingNow) {
							// Sprint state caught up — confirmed, clear flag
							sprintInputEndedEarly = false;
							sprintInputEndedTimer = 0.0f;
						} else {
							sprintInputEndedTimer += lastDeltaTime;
							if (sprintInputEndedTimer > 0.2f) {
								// IsSprinting still true after 200ms — false alarm, clear
								sprintInputEndedEarly = false;
								sprintInputEndedTimer = 0.0f;
							}
						}
					}

					if (sprintingNow && !sprintInputEndedEarly) {
						targetFovOffset = sprintSettings->sprintFovDelta;
					}

					// Stop mode 2 (Speed-Based): scale FOV target by speed ratio during deceleration
					if (sprintSettings->sprintNoiseStopMode == 2 && !sprintingNow && std::abs(currentFovOffset) > 0.01f) {
						targetFovOffset = sprintSettings->sprintFovDelta * sprintSpeedRatio;
					}
				}
			}

			float blendFactor = std::min(sprintSettings->sprintFovBlendSpeed * lastDeltaTime, 0.99f);
			currentFovOffset += (targetFovOffset - currentFovOffset) * blendFactor;
			if (std::abs(currentFovOffset - targetFovOffset) < 0.01f) {
				currentFovOffset = targetFovOffset;
			}
		}

		// Apply FOV offsets (sprint + punch + fall) — must run every frame regardless of
		// spring position/rotation offsets, otherwise the FOV jumps when springs decay to zero.
		if (dynamicBaseFov > 0.0f) {
			float dynamicBase = dynamicBaseFov;

			float fovWithSprint = dynamicBase + currentFovOffset;
			currentFovPunchOffset = fovWithSprint * fovPunchStrength * fovPunchValue;

			float fallFovOffset = fallMgr->GetFovOffset();
			float totalFovOffset = currentFovOffset + currentFovPunchOffset + fallFovOffset;

			float prevWorldFov = a_camera->worldFOV;

			if (std::abs(totalFovOffset) > 0.001f) {
				a_camera->worldFOV = dynamicBase + totalFovOffset;
				lastAppliedFovOffset = totalFovOffset;
			} else {
				a_camera->worldFOV = dynamicBase;
				lastAppliedFovOffset = 0.0f;
			}

			if (Settings::GetSingleton()->debugLogging) {
				float finalFov = a_camera->worldFOV;
				float fovDelta = std::abs(finalFov - prevWorldFov);
				bool hasOffset = std::abs(totalFovOffset) > 0.001f;
				if (fovDelta > 0.05f || (hasOffset && debugFrameCounter % 15 == 0)) {
					logger::info("[FPCameraSettle] FOV: base={:.2f} sprintOfs={:.2f} punchOfs={:.2f} total={:.2f} final={:.2f}",
						dynamicBase, currentFovOffset, currentFovPunchOffset,
						totalFovOffset, finalFov);
				}
			}

			baseFov = dynamicBase;
			baseFovReady = true;
		}

		// OPTIMIZATION: Use squared magnitudes to avoid sqrt
		constexpr float MIN_POS_SQ = 0.001f * 0.001f;
		constexpr float MIN_ROT_SQ = 0.0001f * 0.0001f;
		
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
			cameraNode->local.translate.x += totalPosOffset.x;
			cameraNode->local.translate.y += totalPosOffset.y;
			cameraNode->local.translate.z += totalPosOffset.z;
			
			if (rotMagSq > MIN_ROT_SQ) {
				RE::NiMatrix3 rotMatrix = EulerToMatrix(totalRotOffset.x, totalRotOffset.y, totalRotOffset.z);
				cameraNode->local.rotate = cameraNode->local.rotate * rotMatrix;
			}
			return;
		}
		
		cameraNode->local.translate.x += totalPosOffset.x;
		cameraNode->local.translate.y += totalPosOffset.y;
		cameraNode->local.translate.z += totalPosOffset.z;
		
		cameraNode->world.translate = cameraNode->local.translate;
		cameraNI->world.translate = cameraNode->world.translate;
		
		if (rotMagSq > MIN_ROT_SQ) {
			RE::NiMatrix3 rotMatrix = EulerToMatrix(totalRotOffset.x, totalRotOffset.y, totalRotOffset.z);
			cameraNode->local.rotate = cameraNode->local.rotate * rotMatrix;
			cameraNode->world.rotate = cameraNode->local.rotate;
			cameraNI->world.rotate = cameraNode->world.rotate;
		}
		
		// Update node with dirty flag (like ImprovedCameraSE's Helper::UpdateNode)
		RE::NiUpdateData updateData;
		updateData.flags = RE::NiUpdateData::Flag::kDirty;
		cameraNI->Update(updateData);
		
		// (1P/3P lean skeleton transforms are deferred to animation-thread hooks)
	}

	static int s_skelLogThrottle = 0;

	static const char* kSpineNodeNames[] = {
		"NPC Spine [Spn0]",
		"NPC Spine1 [Spn1]",
		"NPC Spine2 [Spn2]"
	};

	// Frame-gen safe: apply 1P skeleton lean BEFORE the engine's Update() call
	void CameraSettleManager::ApplyLean1P(RE::NiAVObject* a_fpObject)
	{
		auto* settings = Settings::GetSingleton();
		if (!settings->leanEnabled || !settings->leanFirstPersonEnabled || !a_fpObject) return;

		auto* leanMgr = Lean::LeanManager::GetSingleton();
		float leanVal = leanMgr->GetLeanCurrent();
		if (std::abs(leanVal) < 0.001f) return;

		// Safety: verify camera is still in first person before touching skeleton
		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !camera->IsInFirstPerson()) return;

		int nodeIdx = std::clamp(settings->leanFirstPersonNode, 0, 2);
		auto* spineObj = a_fpObject->GetObjectByName(kSpineNodeNames[nodeIdx]);
		// Fallback through other spine bones if the selected one doesn't exist
		if (!spineObj) {
			for (int i = 2; i >= 0; --i) {
				spineObj = a_fpObject->GetObjectByName(kSpineNodeNames[i]);
				if (spineObj) break;
			}
		}
		if (!spineObj) {
			if (settings->debugLogging && s_skelLogThrottle == 0)
				logger::warn("[Lean] 1P: No spine bone found on fpObject");
			return;
		}

		auto* spineNode = spineObj->AsNode();
		if (!spineNode) return;

		float intensity = settings->leanIntensity * settings->leanFirstPersonScale;
		float rollAngle = leanVal * settings->leanRollDegrees * DEG_TO_RAD * intensity;
		float lateralShift = leanVal * settings->leanPosAmount * intensity * 0.3f;

		RE::NiMatrix3 leanRot = EulerToMatrix(0.0f, rollAngle, 0.0f);
		spineNode->local.rotate = spineNode->local.rotate * leanRot;
		spineNode->local.translate.x += lateralShift;

		if (settings->debugLogging && s_skelLogThrottle == 0)
			logger::info("[Lean] 1P applied: lean={:.3f} roll={:.3f}deg shift={:.2f} bone={}",
				leanVal, rollAngle * RAD_TO_DEG, lateralShift, spineObj->name.c_str());

		s_skelLogThrottle = (s_skelLogThrottle + 1) % 60;
	}

	// Third-person body lean — disabled for now (under development)
	void CameraSettleManager::ApplyLean3P([[maybe_unused]] RE::NiAVObject* a_tpObject)
	{
		// Intentionally empty — 3P body lean is not supported in this version
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
		sprintInputEndedEarly = false;
		sprintInputEndedTimer = 0.0f;
		idleNoiseAllowedAfterSprint = true;
		
		// Reset speed-based blending state
		currentSpeed = 0.0f;
		speedBlend = 0.0f;
		movementStartTime = 0.0f;
		wasMovingForGrace = false;
		walkImpulseBlocked = false;
		
		// Reset jump detection state
		didJump = false;
		jumpStartZ = 0.0f;
		
		// Reset performance caches
		cachedNiCamera = nullptr;
		cachedCameraNode = nullptr;
		lastWalkRunBlend = -1.0f;  // Force recalculation
		lastBlendWeaponDrawn = false;
		hotReloadTimer = 0.0f;
		
		// Reset idle noise state
		// Note: We don't reset idleNoisePhase - it continues smoothly
		// Only reset the amplitude so noise fades out naturally
		idleNoiseAmplitude = 0.0f;
		idleNoiseArcheryScale = 1.0f;
		idleNoiseSneakScale = 1.0f;
		idleNoiseOffset = { 0.0f, 0.0f, 0.0f };
		idleNoiseRotation = { 0.0f, 0.0f, 0.0f };
		wasInDialogue = false;
		archeryDrawActive = false;
		archeryReleaseTimer = 0.0f;
		
		// Reset movement noise state (keep phase for smooth re-entry)
		walkNoiseAmplitude = 0.0f;
		runNoiseAmplitude = 0.0f;
		sprintNoiseAmplitude = 0.0f;
		movementNoiseOffset = { 0.0f, 0.0f, 0.0f };
		movementNoiseRotation = { 0.0f, 0.0f, 0.0f };
		
		// Reset speed tracking (keep reference speed — it's learned over time)
		hasLastPlayerPos = false;
		playerWorldSpeed = 0.0f;
		sprintSpeedRatio = 0.0f;
		
		// Reset sprint effects state - undo our FOV contribution
		if (std::abs(lastAppliedFovOffset) > 0.001f) {
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (camera) {
				camera->worldFOV -= lastAppliedFovOffset;
			}
		}
		lastAppliedFovOffset = 0.0f;
		dynamicBaseFov = 0.0f;
		baseFovReady = false;
		currentFovOffset = 0.0f;
		currentBlurStrength = 0.0f;
		fovCaptured = false;
		fovPunchActive = false;
		fovPunchTimer = 0.0f;
		fovPunchStrength = 0.0f;
		fovPunchValue = 0.0f;
		currentFovPunchOffset = 0.0f;
		
		// Stop blur effect if active
		if (blurEffectActive && sprintImod) {
			RE::ImageSpaceModifierInstanceForm::Stop(sprintImod);
			sprintImodInstance = nullptr;
			blurEffectActive = false;
		}
		// Note: Don't destroy sprintImod here - it persists
		
		// Reset fall effect (stops audio + IMOD, clears phase)
		FallEffect::FallEffectManager::GetSingleton()->Reset();
		
		// Reset lean state
		Lean::LeanManager::GetSingleton()->Reset();
		
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
				auto* playerCamera = RE::PlayerCamera::GetSingleton();
				auto* manager = CameraSettleManager::GetSingleton();

				// Strip our offset before the chain so other plugins see the true base FOV
				float stripped = 0.0f;
				if (playerCamera && playerCamera == a_camera) {
					float prevApplied = manager->lastAppliedFovOffset;
					if (std::abs(prevApplied) > 0.001f) {
						playerCamera->worldFOV -= prevApplied;
					}
					stripped = playerCamera->worldFOV;
				}

				_originalCameraUpdate(a_camera);
				
				if (playerCamera && playerCamera == a_camera) {
					float afterChain = playerCamera->worldFOV;
					// If the chain wrote a new value, use it; otherwise use the stripped value
					if (std::abs(afterChain - stripped) > 0.001f) {
						manager->dynamicBaseFov = afterChain;
					} else {
						manager->dynamicBaseFov = stripped;
					}
					manager->ApplyCameraOffset(playerCamera);
				}
			}

			static inline REL::Relocation<decltype(OnCameraUpdate)> _originalCameraUpdate;
		};

		// Animation-thread hook for first-person skeleton (frame-gen safe)
		class UpdateFirstPersonHook
		{
		public:
			static void Install()
			{
				auto& trampoline = SKSE::GetTrampoline();
				REL::Relocation<std::uintptr_t> hook{ RELOCATION_ID(39446, 40522) };
				logger::info("[FPCameraSettle] UpdateFirstPerson hook base: {:X}, offset 0xD7", hook.address());
				_originalFunc = trampoline.write_call<5>(hook.address() + 0xD7, OnFirstPersonUpdate);
				logger::info("[FPCameraSettle] UpdateFirstPerson hook installed (frame-gen safe)");
			}

		private:
			static void OnFirstPersonUpdate(RE::NiAVObject* a_fpObject, RE::NiUpdateData* a_updateData)
			{
				CameraSettleManager::GetSingleton()->ApplyLean1P(a_fpObject);
				_originalFunc(a_fpObject, a_updateData);
			}

			static inline REL::Relocation<decltype(OnFirstPersonUpdate)> _originalFunc;
		};

		// Animation-thread hook for third-person skeleton (frame-gen safe)
		class UpdateThirdPersonHook
		{
		public:
			static void Install()
			{
				auto& trampoline = SKSE::GetTrampoline();
				REL::Relocation<std::uintptr_t> hook{ RELOCATION_ID(39446, 40522) };
				logger::info("[FPCameraSettle] UpdateThirdPerson hook base: {:X}, offset 0x94", hook.address());
				_originalFunc = trampoline.write_call<5>(hook.address() + 0x94, OnThirdPersonUpdate);
				logger::info("[FPCameraSettle] UpdateThirdPerson hook installed (frame-gen safe)");
			}

		private:
			static void OnThirdPersonUpdate(RE::NiAVObject* a_tpObject, RE::NiUpdateData* a_updateData)
			{
				CameraSettleManager::GetSingleton()->ApplyLean3P(a_tpObject);
				_originalFunc(a_tpObject, a_updateData);
			}

			static inline REL::Relocation<decltype(OnThirdPersonUpdate)> _originalFunc;
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
		// Allocate trampoline space (main update + camera update + projectile launch + 1P anim)
		SKSE::GetTrampoline().create(192);
		
		// Install hooks
		Hook::MainUpdateHook::Install();
		Hook::CameraUpdateHook::Install();
		Hook::UpdateFirstPersonHook::Install();
		// 3P hook not installed — third-person body lean is disabled for now
		Lean::InstallProjectileHook();
		
		// Initialize sprint blur IMOD
		InitializeSprintBlurIMOD();
		
		// Initialize fall-effect IMOD (clones one with double-vision + radial blur interpolators)
		FallEffect::FallEffectManager::GetSingleton()->Initialize();
		
		// Register for hit events (explicit cast needed since we inherit from multiple event sinks)
		auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
		if (eventSource) {
			eventSource->AddEventSink<RE::TESHitEvent>(CameraSettleManager::GetSingleton());
			logger::info("[FPCameraSettle] Registered for hit events");
		}
		
		// Register for input events (early sprint button detection)
		auto* inputManager = RE::BSInputDeviceManager::GetSingleton();
		if (inputManager) {
			inputManager->AddEventSink(static_cast<RE::BSTEventSink<RE::InputEvent*>*>(CameraSettleManager::GetSingleton()));
			logger::info("[FPCameraSettle] Registered for input events");
		}
		
		// Register Precision hit callback if available
		CameraSettleManager::GetSingleton()->RegisterPrecisionAPI();
		
		logger::info("[FPCameraSettle] Camera settle system installed");
	}
}

