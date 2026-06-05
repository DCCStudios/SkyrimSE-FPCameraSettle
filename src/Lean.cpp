#include "Lean.h"
#include "Settings.h"

namespace Lean
{
	static int s_leanLogThrottle = 0;

	void LeanManager::Update(float a_delta)
	{
		auto* settings = Settings::GetSingleton();
		bool dbg = settings->debugLogging;

		if (!settings->leanEnabled) {
			if (dbg && s_leanLogThrottle == 0) {
				logger::info("[Lean] Disabled (leanEnabled=false), current={:.3f}", leanCurrent);
			}
			if (std::abs(leanCurrent) > 0.001f) {
				float returnSpeed = settings->leanReturnSpeed;
				float blend = std::min(returnSpeed * a_delta, 0.95f);
				leanCurrent += (0.0f - leanCurrent) * blend;
				if (std::abs(leanCurrent) < 0.001f) leanCurrent = 0.0f;
			}
			s_leanLogThrottle = (s_leanLogThrottle + 1) % 120;
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) return;

		if (ShouldDisableLean(player)) {
			if (dbg && leanSource != LeanSource::None) {
				logger::info("[Lean] ShouldDisableLean → clearing target (was source={})", static_cast<int>(leanSource));
			}
			leanTarget = 0.0f;
			leanSource = LeanSource::None;
			toggledLeft = false;
			toggledRight = false;
		}

		// Contextual lean (only if no manual lean active)
		if (leanSource != LeanSource::Manual && settings->leanContextualEnabled) {
			UpdateContextual(a_delta);
			if (std::abs(contextualTarget) > 0.001f) {
				if (dbg && leanSource != LeanSource::Contextual) {
					logger::info("[Lean] Contextual ENGAGED, target={:.3f}", contextualTarget);
				}
				leanTarget = contextualTarget;
				leanSource = LeanSource::Contextual;
			} else if (leanSource == LeanSource::Contextual) {
				if (dbg) logger::info("[Lean] Contextual DISENGAGED");
				leanTarget = 0.0f;
				leanSource = LeanSource::None;
			}
		} else if (!settings->leanContextualEnabled && leanSource == LeanSource::Contextual) {
			leanTarget = 0.0f;
			leanSource = LeanSource::None;
		}

		// Blend toward target
		float speed = (std::abs(leanTarget) < 0.001f) ? settings->leanReturnSpeed : settings->leanBlendSpeed;
		float blend = std::min(speed * a_delta, 0.95f);
		float prevCurrent = leanCurrent;
		leanCurrent += (leanTarget - leanCurrent) * blend;

		if (std::abs(leanCurrent) < 0.001f && std::abs(leanTarget) < 0.001f) {
			leanCurrent = 0.0f;
		}

		if (dbg && s_leanLogThrottle == 0) {
			const char* srcName = (leanSource == LeanSource::Manual) ? "Manual" :
			                      (leanSource == LeanSource::Contextual) ? "Contextual" : "None";
			logger::info("[Lean] src={} target={:.3f} current={:.3f} speed={:.1f} blend={:.4f}",
				srcName, leanTarget, leanCurrent, speed, blend);
		}
		s_leanLogThrottle = (s_leanLogThrottle + 1) % 60;
	}

	void LeanManager::SetManualLean(float a_target)
	{
		leanTarget = std::clamp(a_target, -1.0f, 1.0f);
		leanSource = LeanSource::Manual;
		if (Settings::GetSingleton()->debugLogging)
			logger::info("[Lean] SetManualLean target={:.2f}", leanTarget);
	}

	void LeanManager::ClearManualLean()
	{
		if (leanSource == LeanSource::Manual) {
			if (Settings::GetSingleton()->debugLogging)
				logger::info("[Lean] ClearManualLean");
			leanTarget = 0.0f;
			leanSource = LeanSource::None;
		}
	}

	void LeanManager::ToggleLeanLeft()
	{
		if (toggledLeft) {
			if (Settings::GetSingleton()->debugLogging)
				logger::info("[Lean] ToggleLeft OFF");
			toggledLeft = false;
			ClearManualLean();
		} else {
			if (Settings::GetSingleton()->debugLogging)
				logger::info("[Lean] ToggleLeft ON");
			toggledLeft = true;
			toggledRight = false;
			SetManualLean(-1.0f);
		}
	}

	void LeanManager::ToggleLeanRight()
	{
		if (toggledRight) {
			if (Settings::GetSingleton()->debugLogging)
				logger::info("[Lean] ToggleRight OFF");
			toggledRight = false;
			ClearManualLean();
		} else {
			if (Settings::GetSingleton()->debugLogging)
				logger::info("[Lean] ToggleRight ON");
			toggledRight = true;
			toggledLeft = false;
			SetManualLean(1.0f);
		}
	}

	void LeanManager::Reset()
	{
		leanTarget = 0.0f;
		leanCurrent = 0.0f;
		leanSource = LeanSource::None;
		toggledLeft = false;
		toggledRight = false;
		contextualTarget = 0.0f;
		contextualHoldTimer = 0.0f;
		contextualWasActive = false;
	}

	bool LeanManager::ShouldDisableLean(RE::PlayerCharacter* a_player) const
	{
		if (!a_player) return true;

		auto* actorState = a_player->AsActorState();
		if (!actorState) return true;

		if (actorState->IsSprinting()) return true;
		if (a_player->IsInMidair()) return true;
		if (actorState->IsSwimming()) return true;
		if (a_player->IsOnMount()) return true;

		if (a_player->GetOccupiedFurniture()) return true;

		auto* ui = RE::UI::GetSingleton();
		if (ui && (ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) ||
		           ui->IsMenuOpen(RE::Console::MENU_NAME))) {
			return true;
		}

		return false;
	}

	bool LeanManager::IsInRangedCombatStance(RE::PlayerCharacter* a_player) const
	{
		auto* settings = Settings::GetSingleton();
		bool dbg = settings->debugLogging;
		auto* actorState = a_player->AsActorState();
		if (!actorState) return false;

		if (!actorState->IsWeaponDrawn()) {
			contextualHoldTimer = 0.0f;
			contextualWasActive = false;
			return false;
		}

		auto attackState = actorState->GetAttackState();
		bool activelyAiming = false;

		// Bow: drawing or fully drawn
		if (settings->leanContextualBow) {
			if (attackState == RE::ATTACK_STATE_ENUM::kBowDraw ||
				attackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
				attackState == RE::ATTACK_STATE_ENUM::kBowDrawn) {
				if (dbg && s_leanLogThrottle == 0)
					logger::info("[Lean] RangedStance: BOW aiming (attackState={})", static_cast<int>(attackState));
				activelyAiming = true;
			}
		}

		// Crossbow: aimed and ready
		if (!activelyAiming && settings->leanContextualCrossbow) {
			auto* weapon = a_player->GetEquippedObject(false);
			if (weapon) {
				auto* weap = weapon->As<RE::TESObjectWEAP>();
				if (weap && weap->IsCrossbow()) {
					if (attackState == RE::ATTACK_STATE_ENUM::kBowDrawn) {
						if (dbg && s_leanLogThrottle == 0)
							logger::info("[Lean] RangedStance: CROSSBOW aimed (attackState={})", static_cast<int>(attackState));
						activelyAiming = true;
					}
				}
			}
		}

		// Magic: actively casting
		if (!activelyAiming && settings->leanContextualMagic) {
			bool castingRight = false;
			bool castingLeft = false;
			a_player->GetGraphVariableBool("IsCastingRight", castingRight);
			a_player->GetGraphVariableBool("IsCastingDual", castingLeft);
			if (castingRight || castingLeft) {
				if (dbg && s_leanLogThrottle == 0)
					logger::info("[Lean] RangedStance: MAGIC casting (R={} L={})", castingRight, castingLeft);
				activelyAiming = true;
			}
		}

		if (activelyAiming) {
			contextualHoldTimer = settings->leanContextualHoldTime;
			contextualWasActive = true;
			return true;
		}

		// Hold timer: keep lean active briefly after aiming ends
		if (contextualWasActive && contextualHoldTimer > 0.0f) {
			if (dbg && s_leanLogThrottle == 0)
				logger::info("[Lean] RangedStance: HOLD timer={:.2f}", contextualHoldTimer);
			return true;
		}

		contextualWasActive = false;

		if (dbg && s_leanLogThrottle == 0)
			logger::info("[Lean] RangedStance: NONE (weaponDrawn=true, attackState={}, bow={} xbow={} magic={})",
				static_cast<int>(attackState), settings->leanContextualBow, settings->leanContextualCrossbow, settings->leanContextualMagic);
		return false;
	}

	bool LeanManager::CastLeanRay(RE::bhkWorld* a_world, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float& a_hitFraction)
	{
		if (!a_world) return false;

		RE::bhkPickData pickData{};
		float scale = RE::bhkWorld::GetWorldScale();

		pickData.rayInput.from = RE::hkVector4(a_from.x * scale, a_from.y * scale, a_from.z * scale, 0.0f);
		pickData.rayInput.to = RE::hkVector4(a_to.x * scale, a_to.y * scale, a_to.z * scale, 0.0f);
		pickData.rayInput.enableShapeCollectionFilter = false;
		pickData.rayInput.filterInfo =
			RE::bhkCollisionFilter::GetSingleton()->GetNewSystemGroup() << 16 |
			static_cast<std::uint32_t>(RE::COL_LAYER::kLOS);

		if (a_world->PickObject(pickData) && pickData.rayOutput.HasHit()) {
			// Filter out actors
			if (pickData.rayOutput.rootCollidable) {
				auto layer = static_cast<RE::COL_LAYER>(
					pickData.rayOutput.rootCollidable->broadPhaseHandle.collisionFilterInfo & 0x7F);
				if (layer == RE::COL_LAYER::kCharController ||
					layer == RE::COL_LAYER::kBiped) {
					return false;
				}
			}
			a_hitFraction = pickData.rayOutput.hitFraction;
			return true;
		}

		return false;
	}

	void LeanManager::UpdateContextual(float a_delta)
	{
		auto* settings = Settings::GetSingleton();
		bool dbg = settings->debugLogging;
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !settings->leanContextualEnabled) {
			contextualTarget = 0.0f;
			contextualHoldTimer = 0.0f;
			contextualWasActive = false;
			return;
		}

		if (settings->leanContextualGamepadOnly) {
			if (!IsLastInputGamepad()) {
				if (dbg && s_leanLogThrottle == 0)
					logger::info("[Lean] Contextual skipped: gamepadOnly=true but last input was keyboard/mouse");
				contextualTarget = 0.0f;
				contextualHoldTimer = 0.0f;
				return;
			}
		}

		if (contextualHoldTimer > 0.0f) {
			contextualHoldTimer -= a_delta;
		}

		if (!IsInRangedCombatStance(player)) {
			contextualTarget = 0.0f;
			return;
		}

		auto* cell = player->GetParentCell();
		auto* bhkWorld = cell ? cell->GetbhkWorld() : nullptr;
		if (!bhkWorld) {
			if (dbg && s_leanLogThrottle == 0)
				logger::info("[Lean] Contextual: no bhkWorld (cell={})", cell != nullptr);
			contextualTarget = 0.0f;
			return;
		}

		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !camera->cameraRoot) {
			contextualTarget = 0.0f;
			return;
		}

		auto* cameraNode = camera->cameraRoot.get();
		RE::NiPoint3 camPos = cameraNode->world.translate;
		RE::NiMatrix3 camRot = cameraNode->world.rotate;

		RE::NiPoint3 forward = { camRot.entry[0][1], camRot.entry[1][1], camRot.entry[2][1] };
		RE::NiPoint3 right = { camRot.entry[0][0], camRot.entry[1][0], camRot.entry[2][0] };

		float offset = settings->leanContextualOffset;
		float maxDist = settings->leanContextualDistance;

		// Center ray — detects thin objects that the side rays miss
		RE::NiPoint3 centerEnd = {
			camPos.x + forward.x * maxDist,
			camPos.y + forward.y * maxDist,
			camPos.z + forward.z * maxDist
		};
		float centerHit = 1.0f;
		bool hitCenter = CastLeanRay(bhkWorld, camPos, centerEnd, centerHit);

		// Left ray
		RE::NiPoint3 leftOrigin = {
			camPos.x - right.x * offset,
			camPos.y - right.y * offset,
			camPos.z - right.z * offset
		};
		RE::NiPoint3 leftEnd = {
			leftOrigin.x + forward.x * maxDist,
			leftOrigin.y + forward.y * maxDist,
			leftOrigin.z + forward.z * maxDist
		};

		// Right ray
		RE::NiPoint3 rightOrigin = {
			camPos.x + right.x * offset,
			camPos.y + right.y * offset,
			camPos.z + right.z * offset
		};
		RE::NiPoint3 rightEnd = {
			rightOrigin.x + forward.x * maxDist,
			rightOrigin.y + forward.y * maxDist,
			rightOrigin.z + forward.z * maxDist
		};

		float leftHit = 1.0f;
		float rightHit = 1.0f;
		bool hitLeft = CastLeanRay(bhkWorld, leftOrigin, leftEnd, leftHit);
		bool hitRight = CastLeanRay(bhkWorld, rightOrigin, rightEnd, rightHit);

		float deadzone = settings->leanContextualDeadzone;

		// Determine lean direction:
		// - Side ray hits one side only → lean toward the open side (wide wall/corner)
		// - Center hits + one side open → lean toward the open side (thin pole/pillar)
		// - Both sides hit (or both miss with no center) → no lean
		bool wallLeft  = hitLeft  && leftHit  < (1.0f - deadzone);
		bool wallRight = hitRight && rightHit < (1.0f - deadzone);
		bool wallCenter = hitCenter && centerHit < (1.0f - deadzone);

		float bestHitFraction = 1.0f;
		if (wallLeft && !wallRight) {
			bestHitFraction = leftHit;
		} else if (wallRight && !wallLeft) {
			bestHitFraction = rightHit;
		} else if (wallCenter && !wallLeft && !wallRight) {
			// Thin object: center hits but both side rays pass around it.
			// Use narrow supplementary rays at half offset to determine which side is more open.
			float halfOff = offset * 0.4f;
			RE::NiPoint3 nlOrig = { camPos.x - right.x * halfOff, camPos.y - right.y * halfOff, camPos.z - right.z * halfOff };
			RE::NiPoint3 nrOrig = { camPos.x + right.x * halfOff, camPos.y + right.y * halfOff, camPos.z + right.z * halfOff };
			RE::NiPoint3 nlEnd  = { nlOrig.x + forward.x * maxDist, nlOrig.y + forward.y * maxDist, nlOrig.z + forward.z * maxDist };
			RE::NiPoint3 nrEnd  = { nrOrig.x + forward.x * maxDist, nrOrig.y + forward.y * maxDist, nrOrig.z + forward.z * maxDist };
			float nlHit = 1.0f, nrHit = 1.0f;
			bool hNL = CastLeanRay(bhkWorld, nlOrig, nlEnd, nlHit);
			bool hNR = CastLeanRay(bhkWorld, nrOrig, nrEnd, nrHit);

			if (hNL && !hNR) {
				wallLeft = true;
				bestHitFraction = nlHit;
			} else if (hNR && !hNL) {
				wallRight = true;
				bestHitFraction = nrHit;
			}
			// If both narrow rays hit, the object is too centered to lean around
		} else if (wallCenter) {
			// Center + one side: use the center hit fraction
			if (wallLeft && !wallRight) bestHitFraction = (leftHit < centerHit) ? leftHit : centerHit;
			else if (wallRight && !wallLeft) bestHitFraction = (rightHit < centerHit) ? rightHit : centerHit;
		}

		if (wallLeft && !wallRight) {
			float proximity = 1.0f - bestHitFraction;
			contextualTarget = std::clamp(proximity / (1.0f - deadzone), 0.0f, 1.0f);
		} else if (wallRight && !wallLeft) {
			float proximity = 1.0f - bestHitFraction;
			contextualTarget = -std::clamp(proximity / (1.0f - deadzone), 0.0f, 1.0f);
		} else {
			contextualTarget = 0.0f;
		}

		if (dbg && s_leanLogThrottle == 0) {
			logger::info("[Lean] Contextual rays: C={} ({:.3f}) L={} ({:.3f}) R={} ({:.3f}) dz={:.2f} → target={:.3f}",
				hitCenter, centerHit, hitLeft, leftHit, hitRight, rightHit, deadzone, contextualTarget);
		}
	}

	namespace ProjectileHook
	{
		using LaunchFunc_t = RE::ProjectileHandle*(*)(RE::ProjectileHandle*, RE::Projectile::LaunchData&);
		static LaunchFunc_t _originalLaunch = nullptr;

		static RE::ProjectileHandle* HookedLaunch(RE::ProjectileHandle* a_result, RE::Projectile::LaunchData& a_data)
		{
			auto* leanMgr = LeanManager::GetSingleton();
			auto* settings = Settings::GetSingleton();

			if (settings->debugLogging && a_data.shooter) {
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (player && a_data.shooter == player) {
					logger::info("[Lean] HookedLaunch fired: leaning={} leanVal={:.3f} spell={} origin=({:.0f},{:.0f},{:.0f})",
						leanMgr->IsLeaning(), leanMgr->GetLeanCurrent(),
						a_data.spell ? a_data.spell->GetName() : "none",
						a_data.origin.x, a_data.origin.y, a_data.origin.z);
				}
			}

			if (settings->leanEnabled && leanMgr->IsLeaning() && a_data.shooter) {
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (player && a_data.shooter == player) {
					auto* camera = RE::PlayerCamera::GetSingleton();
					if (camera && camera->cameraRoot) {
						auto& rot = camera->cameraRoot->world.rotate;
						RE::NiPoint3 camRight   = { rot.entry[0][0], rot.entry[1][0], rot.entry[2][0] };
						RE::NiPoint3 camForward  = { rot.entry[0][1], rot.entry[1][1], rot.entry[2][1] };

						float leanVal = leanMgr->GetLeanCurrent();
						float intensity = settings->leanIntensity;
						// Match the camera offset amount so projectiles originate from
						// where the player sees their weapon/hand (the skeleton lean is
						// visual-only and doesn't affect gameplay node reads).
						float lateralAmount = leanVal * settings->leanPosAmount * intensity;

						a_data.origin.x += camRight.x * lateralAmount;
						a_data.origin.y += camRight.y * lateralAmount;
						a_data.origin.z += camRight.z * lateralAmount;

						// Raycast from camera center to find crosshair target point
						RE::NiPoint3 camPos = camera->cameraRoot->world.translate;
						constexpr float kMaxDist = 10000.0f;
						RE::NiPoint3 targetPoint = {
							camPos.x + camForward.x * kMaxDist,
							camPos.y + camForward.y * kMaxDist,
							camPos.z + camForward.z * kMaxDist
						};

						auto* cell = player->GetParentCell();
						auto* bhkWorld = cell ? cell->GetbhkWorld() : nullptr;
						if (bhkWorld) {
							float scale = RE::bhkWorld::GetWorldScale();
							RE::bhkPickData pickData{};
							pickData.rayInput.from = RE::hkVector4(
								camPos.x * scale, camPos.y * scale, camPos.z * scale, 0.0f);
							pickData.rayInput.to = RE::hkVector4(
								targetPoint.x * scale, targetPoint.y * scale, targetPoint.z * scale, 0.0f);
							pickData.rayInput.enableShapeCollectionFilter = false;
							pickData.rayInput.filterInfo =
								RE::bhkCollisionFilter::GetSingleton()->GetNewSystemGroup() << 16 |
								static_cast<std::uint32_t>(RE::COL_LAYER::kLOS);

							if (bhkWorld->PickObject(pickData) && pickData.rayOutput.HasHit()) {
								float t = pickData.rayOutput.hitFraction;
								targetPoint = {
									camPos.x + camForward.x * kMaxDist * t,
									camPos.y + camForward.y * kMaxDist * t,
									camPos.z + camForward.z * kMaxDist * t
								};
							}
						}

						// Re-aim projectile from shifted origin toward the crosshair target
						RE::NiPoint3 dir = {
							targetPoint.x - a_data.origin.x,
							targetPoint.y - a_data.origin.y,
							targetPoint.z - a_data.origin.z
						};
						float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
						if (len > 0.01f) {
							dir.x /= len;
							dir.y /= len;
							dir.z /= len;

							a_data.angleZ = std::atan2(dir.x, dir.y);
							a_data.angleX = std::asin(-dir.z);
						}

						if (settings->debugLogging) {
							logger::info("[Lean] Projectile: shifted by {:.1f}, origin=({:.0f},{:.0f},{:.0f}) aimed at ({:.0f},{:.0f},{:.0f}), angleZ={:.3f} angleX={:.3f}",
								lateralAmount, a_data.origin.x, a_data.origin.y, a_data.origin.z,
								targetPoint.x, targetPoint.y, targetPoint.z, a_data.angleZ, a_data.angleX);
						}
					}
				}
			}

			return _originalLaunch(a_result, a_data);
		}

		// Scan [funcStart, funcStart+searchRange) for an E8 rel32 CALL that targets 'target'
		static std::uintptr_t FindCallTo(std::uintptr_t funcStart, std::size_t searchRange, std::uintptr_t target)
		{
			auto* bytes = reinterpret_cast<const std::uint8_t*>(funcStart);
			for (std::size_t i = 0; i + 5 <= searchRange; ++i) {
				if (bytes[i] == 0xE8) {
					auto rel = *reinterpret_cast<const std::int32_t*>(&bytes[i + 1]);
					std::uintptr_t callTarget = funcStart + i + 5 + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(rel));
					if (callTarget == target) {
						return funcStart + i;
					}
				}
			}
			return 0;
		}
	}

	void InstallProjectileHook()
	{
		auto& trampoline = SKSE::GetTrampoline();

		// Resolve the address of Projectile::Launch — this is the target we scan for
		REL::Relocation<std::uintptr_t> launchFunc{ RELOCATION_ID(42928, 44108) };
		std::uintptr_t launchAddr = launchFunc.address();
		logger::info("[FPCameraSettle] Projectile::Launch resolved to {:X}", launchAddr);

		bool hooked = false;
		constexpr std::size_t kScanRange = 0x1200;

		// Hook 1: TESObjectWEAP::Fire — handles arrow/bolt projectiles
		// Ref: SkywindProjectiles  REL::ID(17693) + 0xe82 (SE)
		{
			REL::Relocation<std::uintptr_t> weaponFire{ RELOCATION_ID(17693, 18102) };
			logger::info("[FPCameraSettle] TESObjectWEAP::Fire at {:X}", weaponFire.address());
			auto callAddr = ProjectileHook::FindCallTo(weaponFire.address(), kScanRange, launchAddr);
			if (callAddr) {
				ProjectileHook::_originalLaunch = reinterpret_cast<ProjectileHook::LaunchFunc_t>(
					trampoline.write_call<5>(callAddr, ProjectileHook::HookedLaunch));
				hooked = true;
				logger::info("[FPCameraSettle] Projectile hook: weapon fire at {:X} (+{:X})",
					callAddr, callAddr - weaponFire.address());
			} else {
				logger::warn("[FPCameraSettle] Could not find Projectile::Launch call in TESObjectWEAP::Fire (scan range=0x{:X})", kScanRange);
			}
		}

		// Hook 2: ActorMagicCaster::castProjectile — handles spell projectiles
		// Ref: SkywindProjectiles  REL::ID(33672) + 0x377 (SE)
		{
			REL::Relocation<std::uintptr_t> magicCast{ RELOCATION_ID(33672, 34452) };
			logger::info("[FPCameraSettle] ActorMagicCaster::castProjectile at {:X}", magicCast.address());
			auto callAddr = ProjectileHook::FindCallTo(magicCast.address(), kScanRange, launchAddr);
			if (!callAddr) {
				// Fallback: try exact known SE offset
				constexpr std::size_t kSEOffset = 0x377;
				auto candidate = magicCast.address() + kSEOffset;
				auto* bytes = reinterpret_cast<const std::uint8_t*>(candidate);
				if (bytes[0] == 0xE8) {
					auto rel = *reinterpret_cast<const std::int32_t*>(&bytes[1]);
					auto target = candidate + 5 + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(rel));
					logger::info("[FPCameraSettle] Magic fallback: byte at +0x377 is E8, target={:X} (expected {:X})", target, launchAddr);
					if (target == launchAddr) {
						callAddr = candidate;
					}
				} else {
					logger::warn("[FPCameraSettle] Magic fallback: byte at +0x377 is 0x{:02X}, not E8", bytes[0]);
				}
			}
			if (callAddr) {
				auto orig = trampoline.write_call<5>(callAddr, ProjectileHook::HookedLaunch);
				if (!hooked)
					ProjectileHook::_originalLaunch = reinterpret_cast<ProjectileHook::LaunchFunc_t>(orig);
				hooked = true;
				logger::info("[FPCameraSettle] Projectile hook: magic caster at {:X} (+{:X})",
					callAddr, callAddr - magicCast.address());
			} else {
				logger::error("[FPCameraSettle] FAILED to hook magic caster — no Projectile::Launch call found");
			}
		}

		if (hooked) {
			logger::info("[FPCameraSettle] Projectile origin hook installed (Projectile::Launch at {:X})", launchAddr);
		} else {
			logger::error("[FPCameraSettle] FAILED to install ANY projectile origin hook");
		}
	}
}
