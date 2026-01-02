#include "Settings.h"

namespace
{
	constexpr auto INI_PATH = L"Data/SKSE/Plugins/FPCameraSettle.ini";
	
	// Action names for display and INI sections
	const char* ActionNames[] = {
		"WalkForward",
		"WalkBackward",
		"WalkLeft",
		"WalkRight",
		"RunForward",
		"RunBackward",
		"RunLeft",
		"RunRight",
		"SprintForward",
		"Jump",
		"Land",
		"Sneak",
		"UnSneak",
		"TakingHit",
		"Hitting",
		"ArrowRelease"
	};
}

void ActionSettings::Load(CSimpleIniA& a_ini, const char* a_section)
{
	enabled = a_ini.GetBoolValue(a_section, "bEnabled", enabled);
	multiplier = static_cast<float>(a_ini.GetDoubleValue(a_section, "fMultiplier", multiplier));
	multiplier = std::clamp(multiplier, 0.0f, 10.0f);  // Clamp to valid range
	blendTime = static_cast<float>(a_ini.GetDoubleValue(a_section, "fBlendTime", blendTime));
	blendTime = std::clamp(blendTime, 0.0f, 1.0f);  // Clamp to valid range
	stiffness = static_cast<float>(a_ini.GetDoubleValue(a_section, "fStiffness", stiffness));
	damping = static_cast<float>(a_ini.GetDoubleValue(a_section, "fDamping", damping));
	positionStrength = static_cast<float>(a_ini.GetDoubleValue(a_section, "fPositionStrength", positionStrength));
	rotationStrength = static_cast<float>(a_ini.GetDoubleValue(a_section, "fRotationStrength", rotationStrength));
	impulseX = static_cast<float>(a_ini.GetDoubleValue(a_section, "fImpulseX", impulseX));
	impulseY = static_cast<float>(a_ini.GetDoubleValue(a_section, "fImpulseY", impulseY));
	impulseZ = static_cast<float>(a_ini.GetDoubleValue(a_section, "fImpulseZ", impulseZ));
	rotImpulseX = static_cast<float>(a_ini.GetDoubleValue(a_section, "fRotImpulseX", rotImpulseX));
	rotImpulseY = static_cast<float>(a_ini.GetDoubleValue(a_section, "fRotImpulseY", rotImpulseY));
	rotImpulseZ = static_cast<float>(a_ini.GetDoubleValue(a_section, "fRotImpulseZ", rotImpulseZ));
}

void ActionSettings::Save(CSimpleIniA& a_ini, const char* a_section) const
{
	a_ini.SetBoolValue(a_section, "bEnabled", enabled, "; Enable settle effect for this action");
	a_ini.SetDoubleValue(a_section, "fMultiplier", multiplier, "; Per-action intensity multiplier (0.0 - 10.0)");
	a_ini.SetDoubleValue(a_section, "fBlendTime", blendTime, "; Time to blend impulse into spring (0 = instant, up to 1.0 sec)");
	a_ini.SetDoubleValue(a_section, "fStiffness", stiffness, "; Spring stiffness (higher = faster return)");
	a_ini.SetDoubleValue(a_section, "fDamping", damping, "; Damping coefficient (higher = less oscillation)");
	a_ini.SetDoubleValue(a_section, "fPositionStrength", positionStrength, "; Position offset strength");
	a_ini.SetDoubleValue(a_section, "fRotationStrength", rotationStrength, "; Rotation offset strength (degrees)");
	a_ini.SetDoubleValue(a_section, "fImpulseX", impulseX, "; Initial X impulse (left/right)");
	a_ini.SetDoubleValue(a_section, "fImpulseY", impulseY, "; Initial Y impulse (forward/back)");
	a_ini.SetDoubleValue(a_section, "fImpulseZ", impulseZ, "; Initial Z impulse (up/down)");
	a_ini.SetDoubleValue(a_section, "fRotImpulseX", rotImpulseX, "; Pitch impulse (+look up, -look down)");
	a_ini.SetDoubleValue(a_section, "fRotImpulseY", rotImpulseY, "; Roll impulse (+tilt right, -tilt left)");
	a_ini.SetDoubleValue(a_section, "fRotImpulseZ", rotImpulseZ, "; Yaw impulse (+look left, -look right)");
}

void ActionSettings::CopyFrom(const ActionSettings& other)
{
	enabled = other.enabled;
	multiplier = other.multiplier;
	blendTime = other.blendTime;
	stiffness = other.stiffness;
	damping = other.damping;
	positionStrength = other.positionStrength;
	rotationStrength = other.rotationStrength;
	impulseX = other.impulseX;
	impulseY = other.impulseY;
	impulseZ = other.impulseZ;
	rotImpulseX = other.rotImpulseX;
	rotImpulseY = other.rotImpulseY;
	rotImpulseZ = other.rotImpulseZ;
}

ActionSettings ActionSettings::Blend(const ActionSettings& a, const ActionSettings& b, float t)
{
	t = std::clamp(t, 0.0f, 1.0f);
	float invT = 1.0f - t;
	
	ActionSettings result;
	// Use the enabled state of whichever has higher weight, or both if equal
	result.enabled = t < 0.5f ? a.enabled : b.enabled;
	result.multiplier = a.multiplier * invT + b.multiplier * t;
	result.blendTime = a.blendTime * invT + b.blendTime * t;
	result.stiffness = a.stiffness * invT + b.stiffness * t;
	result.damping = a.damping * invT + b.damping * t;
	result.positionStrength = a.positionStrength * invT + b.positionStrength * t;
	result.rotationStrength = a.rotationStrength * invT + b.rotationStrength * t;
	result.impulseX = a.impulseX * invT + b.impulseX * t;
	result.impulseY = a.impulseY * invT + b.impulseY * t;
	result.impulseZ = a.impulseZ * invT + b.impulseZ * t;
	result.rotImpulseX = a.rotImpulseX * invT + b.rotImpulseX * t;
	result.rotImpulseY = a.rotImpulseY * invT + b.rotImpulseY * t;
	result.rotImpulseZ = a.rotImpulseZ * invT + b.rotImpulseZ * t;
	return result;
}

const char* Settings::GetActionName(ActionType a_type)
{
	int idx = static_cast<int>(a_type);
	if (idx >= 0 && idx < static_cast<int>(ActionType::kTotal)) {
		return ActionNames[idx];
	}
	return "Unknown";
}

void Settings::InitializeDefaults()
{
	// Walk actions - subtle camera sway
	auto initWalk = [](ActionSettings& s, float xDir, float yDir) {
		s.enabled = true;
		s.multiplier = 1.0f;
		s.blendTime = 0.1f;  // Smooth blend in
		s.stiffness = 80.0f;
		s.damping = 6.0f;
		s.positionStrength = 2.0f;
		s.rotationStrength = 1.5f;
		s.impulseX = xDir * 3.0f;
		s.impulseY = yDir * 2.0f;
		s.impulseZ = 0.5f;
		s.rotImpulseX = yDir * 0.5f;  // Subtle pitch when moving forward/back
		s.rotImpulseY = xDir * 0.3f;  // Subtle yaw when strafing
		s.rotImpulseZ = xDir * 0.8f;  // Roll when strafing
	};
	
	// Run actions - more pronounced
	auto initRun = [](ActionSettings& s, float xDir, float yDir) {
		s.enabled = true;
		s.multiplier = 1.0f;
		s.blendTime = 0.08f;  // Slightly faster blend for running
		s.stiffness = 100.0f;
		s.damping = 7.0f;
		s.positionStrength = 4.0f;
		s.rotationStrength = 2.5f;
		s.impulseX = xDir * 5.0f;
		s.impulseY = yDir * 3.0f;
		s.impulseZ = 1.0f;
		s.rotImpulseX = yDir * 1.0f;
		s.rotImpulseY = xDir * 0.5f;
		s.rotImpulseZ = xDir * 1.5f;
	};
	
	// Initialize walk settings (drawn)
	initWalk(walkForwardDrawn, 0.0f, 1.0f);
	initWalk(walkBackwardDrawn, 0.0f, -1.0f);
	initWalk(walkLeftDrawn, -1.0f, 0.0f);
	initWalk(walkRightDrawn, 1.0f, 0.0f);
	
	// Initialize run settings (drawn)
	initRun(runForwardDrawn, 0.0f, 1.0f);
	initRun(runBackwardDrawn, 0.0f, -1.0f);
	initRun(runLeftDrawn, -1.0f, 0.0f);
	initRun(runRightDrawn, 1.0f, 0.0f);
	
	// Sprint - strong forward momentum settle
	sprintForwardDrawn.enabled = true;
	sprintForwardDrawn.stiffness = 60.0f;
	sprintForwardDrawn.damping = 5.0f;
	sprintForwardDrawn.positionStrength = 8.0f;
	sprintForwardDrawn.rotationStrength = 4.0f;
	sprintForwardDrawn.impulseX = 0.0f;
	sprintForwardDrawn.impulseY = 10.0f;
	sprintForwardDrawn.impulseZ = -3.0f;
	sprintForwardDrawn.rotImpulseX = 3.0f;  // Forward tilt
	sprintForwardDrawn.rotImpulseY = 0.0f;
	sprintForwardDrawn.rotImpulseZ = 0.0f;
	
	// Jump - upward momentum, then settle
	jumpDrawn.enabled = true;
	jumpDrawn.stiffness = 40.0f;
	jumpDrawn.damping = 3.0f;
	jumpDrawn.positionStrength = 6.0f;
	jumpDrawn.rotationStrength = 3.0f;
	jumpDrawn.impulseX = 0.0f;
	jumpDrawn.impulseY = 4.0f;
	jumpDrawn.impulseZ = 8.0f;
	jumpDrawn.rotImpulseX = -2.0f;  // Look up slightly
	jumpDrawn.rotImpulseY = 0.0f;
	jumpDrawn.rotImpulseZ = 0.0f;
	
	// Land - downward compression, then settle
	landDrawn.enabled = true;
	landDrawn.stiffness = 120.0f;
	landDrawn.damping = 10.0f;
	landDrawn.positionStrength = 10.0f;
	landDrawn.rotationStrength = 5.0f;
	landDrawn.impulseX = 0.0f;
	landDrawn.impulseY = 2.0f;
	landDrawn.impulseZ = -12.0f;
	landDrawn.rotImpulseX = 4.0f;  // Downward nod
	landDrawn.rotImpulseY = 0.0f;
	landDrawn.rotImpulseZ = 0.0f;
	
	// Sneak - slow settle down
	sneakDrawn.enabled = true;
	sneakDrawn.stiffness = 50.0f;
	sneakDrawn.damping = 8.0f;
	sneakDrawn.positionStrength = 4.0f;
	sneakDrawn.rotationStrength = 2.0f;
	sneakDrawn.impulseX = 0.0f;
	sneakDrawn.impulseY = 1.0f;
	sneakDrawn.impulseZ = -5.0f;
	sneakDrawn.rotImpulseX = 2.0f;
	sneakDrawn.rotImpulseY = 0.0f;
	sneakDrawn.rotImpulseZ = 0.0f;
	
	// Un-Sneak - rise up
	unSneakDrawn.enabled = true;
	unSneakDrawn.stiffness = 60.0f;
	unSneakDrawn.damping = 7.0f;
	unSneakDrawn.positionStrength = 4.0f;
	unSneakDrawn.rotationStrength = 2.0f;
	unSneakDrawn.impulseX = 0.0f;
	unSneakDrawn.impulseY = -1.0f;
	unSneakDrawn.impulseZ = 4.0f;
	unSneakDrawn.rotImpulseX = -1.5f;
	unSneakDrawn.rotImpulseY = 0.0f;
	unSneakDrawn.rotImpulseZ = 0.0f;
	
	// Taking hit - violent shake
	takingHitDrawn.enabled = true;
	takingHitDrawn.stiffness = 150.0f;
	takingHitDrawn.damping = 12.0f;
	takingHitDrawn.positionStrength = 12.0f;
	takingHitDrawn.rotationStrength = 8.0f;
	takingHitDrawn.impulseX = 0.0f;  // Direction will be set based on hit
	takingHitDrawn.impulseY = -5.0f;
	takingHitDrawn.impulseZ = -3.0f;
	takingHitDrawn.rotImpulseX = 5.0f;
	takingHitDrawn.rotImpulseY = 0.0f;
	takingHitDrawn.rotImpulseZ = 3.0f;
	
	// Hitting something - recoil
	hittingDrawn.enabled = true;
	hittingDrawn.stiffness = 180.0f;
	hittingDrawn.damping = 14.0f;
	hittingDrawn.positionStrength = 6.0f;
	hittingDrawn.rotationStrength = 4.0f;
	hittingDrawn.impulseX = 0.0f;
	hittingDrawn.impulseY = -4.0f;
	hittingDrawn.impulseZ = 2.0f;
	hittingDrawn.rotImpulseX = -2.0f;
	hittingDrawn.rotImpulseY = 0.0f;
	hittingDrawn.rotImpulseZ = 0.0f;
	
	// Arrow release - bow/crossbow shot recoil
	arrowReleaseDrawn.enabled = true;
	arrowReleaseDrawn.multiplier = 1.0f;
	arrowReleaseDrawn.blendTime = 0.0f;  // Instant for snappy bow feel
	arrowReleaseDrawn.stiffness = 150.0f;
	arrowReleaseDrawn.damping = 10.0f;
	arrowReleaseDrawn.positionStrength = 5.0f;
	arrowReleaseDrawn.rotationStrength = 4.0f;
	arrowReleaseDrawn.impulseX = 0.0f;
	arrowReleaseDrawn.impulseY = -3.0f;   // Slight backward push
	arrowReleaseDrawn.impulseZ = 2.0f;    // Slight upward kick
	arrowReleaseDrawn.rotImpulseX = -3.0f; // Pitch up from recoil
	arrowReleaseDrawn.rotImpulseY = 0.0f;
	arrowReleaseDrawn.rotImpulseZ = 0.0f;
	
	// Initialize sheathed versions (same as drawn but will be scaled by weaponSheathedMult)
	walkForwardSheathed = walkForwardDrawn;
	walkBackwardSheathed = walkBackwardDrawn;
	walkLeftSheathed = walkLeftDrawn;
	walkRightSheathed = walkRightDrawn;
	runForwardSheathed = runForwardDrawn;
	runBackwardSheathed = runBackwardDrawn;
	runLeftSheathed = runLeftDrawn;
	runRightSheathed = runRightDrawn;
	sprintForwardSheathed = sprintForwardDrawn;
	jumpSheathed = jumpDrawn;
	landSheathed = landDrawn;
	sneakSheathed = sneakDrawn;
	unSneakSheathed = unSneakDrawn;
	takingHitSheathed = takingHitDrawn;
	hittingSheathed = hittingDrawn;
	arrowReleaseSheathed = arrowReleaseDrawn;
}

void Settings::Load()
{
	// Initialize with sensible defaults first
	InitializeDefaults();
	
	CSimpleIniA ini;
	ini.SetUnicode();
	
	SI_Error rc = ini.LoadFile(INI_PATH);
	if (rc < 0) {
		logger::info("[FPCameraSettle] No INI file found, creating with defaults");
		Save();
		return;
	}
	
	// Load general settings
	enabled = ini.GetBoolValue("General", "bEnabled", enabled);
	globalIntensity = static_cast<float>(ini.GetDoubleValue("General", "fGlobalIntensity", globalIntensity));
	smoothingFactor = static_cast<float>(ini.GetDoubleValue("General", "fSmoothingFactor", smoothingFactor));
	resetOnPause = ini.GetBoolValue("General", "bResetOnPause", resetOnPause);
	springSubsteps = static_cast<int>(ini.GetLongValue("General", "iSpringSubsteps", springSubsteps));
	springSubsteps = std::clamp(springSubsteps, 1, 8);
	
	// Load weapon state settings
	weaponDrawnEnabled = ini.GetBoolValue("WeaponState", "bWeaponDrawnEnabled", weaponDrawnEnabled);
	weaponSheathedEnabled = ini.GetBoolValue("WeaponState", "bWeaponSheathedEnabled", weaponSheathedEnabled);
	weaponDrawnMult = static_cast<float>(ini.GetDoubleValue("WeaponState", "fWeaponDrawnMult", weaponDrawnMult));
	weaponSheathedMult = static_cast<float>(ini.GetDoubleValue("WeaponState", "fWeaponSheathedMult", weaponSheathedMult));
	
	// Load settling behavior
	settleDelay = static_cast<float>(ini.GetDoubleValue("Settling", "fSettleDelay", settleDelay));
	settleSpeed = static_cast<float>(ini.GetDoubleValue("Settling", "fSettleSpeed", settleSpeed));
	settleDampingMult = static_cast<float>(ini.GetDoubleValue("Settling", "fSettleDampingMult", settleDampingMult));
	
	// Load idle noise settings (weapon drawn)
	idleNoiseEnabledDrawn = ini.GetBoolValue("IdleNoise_Drawn", "bEnabled", idleNoiseEnabledDrawn);
	idleNoisePosAmpXDrawn = static_cast<float>(ini.GetDoubleValue("IdleNoise_Drawn", "fPosAmpX", idleNoisePosAmpXDrawn));
	idleNoisePosAmpYDrawn = static_cast<float>(ini.GetDoubleValue("IdleNoise_Drawn", "fPosAmpY", idleNoisePosAmpYDrawn));
	idleNoisePosAmpZDrawn = static_cast<float>(ini.GetDoubleValue("IdleNoise_Drawn", "fPosAmpZ", idleNoisePosAmpZDrawn));
	idleNoiseRotAmpXDrawn = static_cast<float>(ini.GetDoubleValue("IdleNoise_Drawn", "fRotAmpX", idleNoiseRotAmpXDrawn));
	idleNoiseRotAmpYDrawn = static_cast<float>(ini.GetDoubleValue("IdleNoise_Drawn", "fRotAmpY", idleNoiseRotAmpYDrawn));
	idleNoiseRotAmpZDrawn = static_cast<float>(ini.GetDoubleValue("IdleNoise_Drawn", "fRotAmpZ", idleNoiseRotAmpZDrawn));
	idleNoiseFrequencyDrawn = static_cast<float>(ini.GetDoubleValue("IdleNoise_Drawn", "fFrequency", idleNoiseFrequencyDrawn));
	
	// Load idle noise settings (weapon sheathed)
	idleNoiseEnabledSheathed = ini.GetBoolValue("IdleNoise_Sheathed", "bEnabled", idleNoiseEnabledSheathed);
	idleNoisePosAmpXSheathed = static_cast<float>(ini.GetDoubleValue("IdleNoise_Sheathed", "fPosAmpX", idleNoisePosAmpXSheathed));
	idleNoisePosAmpYSheathed = static_cast<float>(ini.GetDoubleValue("IdleNoise_Sheathed", "fPosAmpY", idleNoisePosAmpYSheathed));
	idleNoisePosAmpZSheathed = static_cast<float>(ini.GetDoubleValue("IdleNoise_Sheathed", "fPosAmpZ", idleNoisePosAmpZSheathed));
	idleNoiseRotAmpXSheathed = static_cast<float>(ini.GetDoubleValue("IdleNoise_Sheathed", "fRotAmpX", idleNoiseRotAmpXSheathed));
	idleNoiseRotAmpYSheathed = static_cast<float>(ini.GetDoubleValue("IdleNoise_Sheathed", "fRotAmpY", idleNoiseRotAmpYSheathed));
	idleNoiseRotAmpZSheathed = static_cast<float>(ini.GetDoubleValue("IdleNoise_Sheathed", "fRotAmpZ", idleNoiseRotAmpZSheathed));
	idleNoiseFrequencySheathed = static_cast<float>(ini.GetDoubleValue("IdleNoise_Sheathed", "fFrequency", idleNoiseFrequencySheathed));
	
	// Load sprint effects settings
	sprintFovEnabled = ini.GetBoolValue("SprintEffects", "bFovEnabled", sprintFovEnabled);
	sprintFovDelta = static_cast<float>(ini.GetDoubleValue("SprintEffects", "fFovDelta", sprintFovDelta));
	sprintFovBlendSpeed = static_cast<float>(ini.GetDoubleValue("SprintEffects", "fFovBlendSpeed", sprintFovBlendSpeed));
	sprintBlurEnabled = ini.GetBoolValue("SprintEffects", "bBlurEnabled", sprintBlurEnabled);
	sprintBlurStrength = static_cast<float>(ini.GetDoubleValue("SprintEffects", "fBlurStrength", sprintBlurStrength));
	sprintBlurBlendSpeed = static_cast<float>(ini.GetDoubleValue("SprintEffects", "fBlurBlendSpeed", sprintBlurBlendSpeed));
	sprintBlurRampUp = static_cast<float>(ini.GetDoubleValue("SprintEffects", "fBlurRampUp", sprintBlurRampUp));
	sprintBlurRampDown = static_cast<float>(ini.GetDoubleValue("SprintEffects", "fBlurRampDown", sprintBlurRampDown));
	sprintBlurRadius = static_cast<float>(ini.GetDoubleValue("SprintEffects", "fBlurRadius", sprintBlurRadius));
	
	// Load debug settings
	debugLogging = ini.GetBoolValue("Debug", "bDebugLogging", debugLogging);
	debugOnScreen = ini.GetBoolValue("Debug", "bDebugOnScreen", debugOnScreen);
	enableHotReload = ini.GetBoolValue("Debug", "bEnableHotReload", enableHotReload);
	hotReloadIntervalSec = static_cast<float>(ini.GetDoubleValue("Debug", "fHotReloadInterval", hotReloadIntervalSec));
	
	// Load per-action settings (weapon drawn)
	walkForwardDrawn.Load(ini, "WalkForward_Drawn");
	walkBackwardDrawn.Load(ini, "WalkBackward_Drawn");
	walkLeftDrawn.Load(ini, "WalkLeft_Drawn");
	walkRightDrawn.Load(ini, "WalkRight_Drawn");
	runForwardDrawn.Load(ini, "RunForward_Drawn");
	runBackwardDrawn.Load(ini, "RunBackward_Drawn");
	runLeftDrawn.Load(ini, "RunLeft_Drawn");
	runRightDrawn.Load(ini, "RunRight_Drawn");
	sprintForwardDrawn.Load(ini, "SprintForward_Drawn");
	jumpDrawn.Load(ini, "Jump_Drawn");
	landDrawn.Load(ini, "Land_Drawn");
	sneakDrawn.Load(ini, "Sneak_Drawn");
	unSneakDrawn.Load(ini, "UnSneak_Drawn");
	takingHitDrawn.Load(ini, "TakingHit_Drawn");
	hittingDrawn.Load(ini, "Hitting_Drawn");
	arrowReleaseDrawn.Load(ini, "ArrowRelease_Drawn");
	
	// Load per-action settings (weapon sheathed)
	walkForwardSheathed.Load(ini, "WalkForward_Sheathed");
	walkBackwardSheathed.Load(ini, "WalkBackward_Sheathed");
	walkLeftSheathed.Load(ini, "WalkLeft_Sheathed");
	walkRightSheathed.Load(ini, "WalkRight_Sheathed");
	runForwardSheathed.Load(ini, "RunForward_Sheathed");
	runBackwardSheathed.Load(ini, "RunBackward_Sheathed");
	runLeftSheathed.Load(ini, "RunLeft_Sheathed");
	runRightSheathed.Load(ini, "RunRight_Sheathed");
	sprintForwardSheathed.Load(ini, "SprintForward_Sheathed");
	jumpSheathed.Load(ini, "Jump_Sheathed");
	landSheathed.Load(ini, "Land_Sheathed");
	sneakSheathed.Load(ini, "Sneak_Sheathed");
	unSneakSheathed.Load(ini, "UnSneak_Sheathed");
	takingHitSheathed.Load(ini, "TakingHit_Sheathed");
	hittingSheathed.Load(ini, "Hitting_Sheathed");
	arrowReleaseSheathed.Load(ini, "ArrowRelease_Sheathed");
	
	// Track file modification time
	try {
		lastModifiedTime = std::filesystem::last_write_time(INI_PATH);
	} catch (...) {
		// File doesn't exist, will be created on save
	}
	
	// Increment version to invalidate caches
	settingsVersion++;
	
	logger::info("[FPCameraSettle] Settings loaded from INI");
}

void Settings::Save()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	
	// General settings
	ini.SetBoolValue("General", "bEnabled", enabled, "; Master toggle for all camera settle effects");
	ini.SetDoubleValue("General", "fGlobalIntensity", globalIntensity, "; Global intensity multiplier (1.0 = normal)");
	ini.SetDoubleValue("General", "fSmoothingFactor", smoothingFactor, "; Input smoothing (0 = none, 1 = maximum)");
	ini.SetBoolValue("General", "bResetOnPause", resetOnPause, "; Disable camera effects when game is paused (menus, console, etc.)");
	ini.SetLongValue("General", "iSpringSubsteps", springSubsteps, "; Number of physics sub-steps per frame (1-8, higher = more stable but slower)");
	
	// Weapon state settings
	ini.SetBoolValue("WeaponState", "bWeaponDrawnEnabled", weaponDrawnEnabled, "; Enable effects when weapon is drawn");
	ini.SetBoolValue("WeaponState", "bWeaponSheathedEnabled", weaponSheathedEnabled, "; Enable effects when weapon is sheathed");
	ini.SetDoubleValue("WeaponState", "fWeaponDrawnMult", weaponDrawnMult, "; Effect multiplier when weapon is drawn");
	ini.SetDoubleValue("WeaponState", "fWeaponSheathedMult", weaponSheathedMult, "; Effect multiplier when weapon is sheathed");
	
	// Settling behavior
	ini.SetDoubleValue("Settling", "fSettleDelay", settleDelay, "; Delay before settling starts (seconds)");
	ini.SetDoubleValue("Settling", "fSettleSpeed", settleSpeed, "; How fast settling occurs");
	ini.SetDoubleValue("Settling", "fSettleDampingMult", settleDampingMult, "; Max damping multiplier when settled");
	
	// Idle noise settings (weapon drawn)
	ini.SetBoolValue("IdleNoise_Drawn", "bEnabled", idleNoiseEnabledDrawn, "; Enable subtle camera motion when standing idle (weapon drawn)");
	ini.SetDoubleValue("IdleNoise_Drawn", "fPosAmpX", idleNoisePosAmpXDrawn, "; Position amplitude X (left/right)");
	ini.SetDoubleValue("IdleNoise_Drawn", "fPosAmpY", idleNoisePosAmpYDrawn, "; Position amplitude Y (forward/back)");
	ini.SetDoubleValue("IdleNoise_Drawn", "fPosAmpZ", idleNoisePosAmpZDrawn, "; Position amplitude Z (up/down breathing)");
	ini.SetDoubleValue("IdleNoise_Drawn", "fRotAmpX", idleNoiseRotAmpXDrawn, "; Rotation amplitude pitch (degrees)");
	ini.SetDoubleValue("IdleNoise_Drawn", "fRotAmpY", idleNoiseRotAmpYDrawn, "; Rotation amplitude roll (degrees)");
	ini.SetDoubleValue("IdleNoise_Drawn", "fRotAmpZ", idleNoiseRotAmpZDrawn, "; Rotation amplitude yaw (degrees)");
	ini.SetDoubleValue("IdleNoise_Drawn", "fFrequency", idleNoiseFrequencyDrawn, "; Noise frequency (cycles per second)");
	
	// Idle noise settings (weapon sheathed)
	ini.SetBoolValue("IdleNoise_Sheathed", "bEnabled", idleNoiseEnabledSheathed, "; Enable subtle camera motion when standing idle (weapon sheathed)");
	ini.SetDoubleValue("IdleNoise_Sheathed", "fPosAmpX", idleNoisePosAmpXSheathed, "; Position amplitude X (left/right)");
	ini.SetDoubleValue("IdleNoise_Sheathed", "fPosAmpY", idleNoisePosAmpYSheathed, "; Position amplitude Y (forward/back)");
	ini.SetDoubleValue("IdleNoise_Sheathed", "fPosAmpZ", idleNoisePosAmpZSheathed, "; Position amplitude Z (up/down breathing)");
	ini.SetDoubleValue("IdleNoise_Sheathed", "fRotAmpX", idleNoiseRotAmpXSheathed, "; Rotation amplitude pitch (degrees)");
	ini.SetDoubleValue("IdleNoise_Sheathed", "fRotAmpY", idleNoiseRotAmpYSheathed, "; Rotation amplitude roll (degrees)");
	ini.SetDoubleValue("IdleNoise_Sheathed", "fRotAmpZ", idleNoiseRotAmpZSheathed, "; Rotation amplitude yaw (degrees)");
	ini.SetDoubleValue("IdleNoise_Sheathed", "fFrequency", idleNoiseFrequencySheathed, "; Noise frequency (cycles per second)");
	
	// Sprint effects
	ini.SetBoolValue("SprintEffects", "bFovEnabled", sprintFovEnabled, "; Enable FOV increase when sprinting");
	ini.SetDoubleValue("SprintEffects", "fFovDelta", sprintFovDelta, "; FOV increase when sprinting (degrees)");
	ini.SetDoubleValue("SprintEffects", "fFovBlendSpeed", sprintFovBlendSpeed, "; How fast to blend FOV (higher = faster)");
	ini.SetBoolValue("SprintEffects", "bBlurEnabled", sprintBlurEnabled, "; Enable radial blur when sprinting");
	ini.SetDoubleValue("SprintEffects", "fBlurStrength", sprintBlurStrength, "; Radial blur strength (0-1)");
	ini.SetDoubleValue("SprintEffects", "fBlurBlendSpeed", sprintBlurBlendSpeed, "; How fast to blend blur (higher = faster)");
	ini.SetDoubleValue("SprintEffects", "fBlurRampUp", sprintBlurRampUp, "; IMOD ramp up time in seconds (how fast blur appears)");
	ini.SetDoubleValue("SprintEffects", "fBlurRampDown", sprintBlurRampDown, "; IMOD ramp down time in seconds (how fast blur fades)");
	ini.SetDoubleValue("SprintEffects", "fBlurRadius", sprintBlurRadius, "; Blur start radius (0 = blur from center, 1 = edges only)");
	
	// Debug settings
	ini.SetBoolValue("Debug", "bDebugLogging", debugLogging, "; Enable detailed debug logging");
	ini.SetBoolValue("Debug", "bDebugOnScreen", debugOnScreen, "; Show debug info on screen");
	ini.SetBoolValue("Debug", "bEnableHotReload", enableHotReload, "; Auto-reload INI when changed");
	ini.SetDoubleValue("Debug", "fHotReloadInterval", hotReloadIntervalSec, "; Hot reload check interval (seconds)");
	
	// Per-action settings (weapon drawn)
	walkForwardDrawn.Save(ini, "WalkForward_Drawn");
	walkBackwardDrawn.Save(ini, "WalkBackward_Drawn");
	walkLeftDrawn.Save(ini, "WalkLeft_Drawn");
	walkRightDrawn.Save(ini, "WalkRight_Drawn");
	runForwardDrawn.Save(ini, "RunForward_Drawn");
	runBackwardDrawn.Save(ini, "RunBackward_Drawn");
	runLeftDrawn.Save(ini, "RunLeft_Drawn");
	runRightDrawn.Save(ini, "RunRight_Drawn");
	sprintForwardDrawn.Save(ini, "SprintForward_Drawn");
	jumpDrawn.Save(ini, "Jump_Drawn");
	landDrawn.Save(ini, "Land_Drawn");
	sneakDrawn.Save(ini, "Sneak_Drawn");
	unSneakDrawn.Save(ini, "UnSneak_Drawn");
	takingHitDrawn.Save(ini, "TakingHit_Drawn");
	hittingDrawn.Save(ini, "Hitting_Drawn");
	arrowReleaseDrawn.Save(ini, "ArrowRelease_Drawn");
	
	// Per-action settings (weapon sheathed)
	walkForwardSheathed.Save(ini, "WalkForward_Sheathed");
	walkBackwardSheathed.Save(ini, "WalkBackward_Sheathed");
	walkLeftSheathed.Save(ini, "WalkLeft_Sheathed");
	walkRightSheathed.Save(ini, "WalkRight_Sheathed");
	runForwardSheathed.Save(ini, "RunForward_Sheathed");
	runBackwardSheathed.Save(ini, "RunBackward_Sheathed");
	runLeftSheathed.Save(ini, "RunLeft_Sheathed");
	runRightSheathed.Save(ini, "RunRight_Sheathed");
	sprintForwardSheathed.Save(ini, "SprintForward_Sheathed");
	jumpSheathed.Save(ini, "Jump_Sheathed");
	landSheathed.Save(ini, "Land_Sheathed");
	sneakSheathed.Save(ini, "Sneak_Sheathed");
	unSneakSheathed.Save(ini, "UnSneak_Sheathed");
	takingHitSheathed.Save(ini, "TakingHit_Sheathed");
	hittingSheathed.Save(ini, "Hitting_Sheathed");
	arrowReleaseSheathed.Save(ini, "ArrowRelease_Sheathed");
	
	SI_Error rc = ini.SaveFile(INI_PATH);
	if (rc < 0) {
		logger::error("[FPCameraSettle] Failed to save INI file");
	} else {
		logger::info("[FPCameraSettle] Settings saved to INI");
	}
}

void Settings::CheckForReload(float a_deltaTime)
{
	if (!enableHotReload) {
		return;
	}
	
	timeSinceLastCheck += a_deltaTime;
	if (timeSinceLastCheck < hotReloadIntervalSec) {
		return;
	}
	timeSinceLastCheck = 0.0f;
	
	try {
		auto currentTime = std::filesystem::last_write_time(INI_PATH);
		if (currentTime != lastModifiedTime) {
			lastModifiedTime = currentTime;
			Load();
			logger::info("[FPCameraSettle] Settings reloaded (hot reload)");
		}
	} catch (...) {
		// File doesn't exist or can't be accessed
	}
}

ActionSettings& Settings::GetActionSettings(ActionType a_type)
{
	// Default to drawn settings
	return GetActionSettingsForState(a_type, true);
}

const ActionSettings& Settings::GetActionSettings(ActionType a_type) const
{
	return GetActionSettingsForState(a_type, true);
}

ActionSettings& Settings::GetActionSettingsForState(ActionType a_type, bool a_weaponDrawn)
{
	if (a_weaponDrawn) {
		switch (a_type) {
		case ActionType::WalkForward: return walkForwardDrawn;
		case ActionType::WalkBackward: return walkBackwardDrawn;
		case ActionType::WalkLeft: return walkLeftDrawn;
		case ActionType::WalkRight: return walkRightDrawn;
		case ActionType::RunForward: return runForwardDrawn;
		case ActionType::RunBackward: return runBackwardDrawn;
		case ActionType::RunLeft: return runLeftDrawn;
		case ActionType::RunRight: return runRightDrawn;
		case ActionType::SprintForward: return sprintForwardDrawn;
		case ActionType::Jump: return jumpDrawn;
		case ActionType::Land: return landDrawn;
		case ActionType::Sneak: return sneakDrawn;
		case ActionType::UnSneak: return unSneakDrawn;
		case ActionType::TakingHit: return takingHitDrawn;
		case ActionType::Hitting: return hittingDrawn;
		case ActionType::ArrowRelease: return arrowReleaseDrawn;
		default: return walkForwardDrawn;
		}
	} else {
		switch (a_type) {
		case ActionType::WalkForward: return walkForwardSheathed;
		case ActionType::WalkBackward: return walkBackwardSheathed;
		case ActionType::WalkLeft: return walkLeftSheathed;
		case ActionType::WalkRight: return walkRightSheathed;
		case ActionType::RunForward: return runForwardSheathed;
		case ActionType::RunBackward: return runBackwardSheathed;
		case ActionType::RunLeft: return runLeftSheathed;
		case ActionType::RunRight: return runRightSheathed;
		case ActionType::SprintForward: return sprintForwardSheathed;
		case ActionType::Jump: return jumpSheathed;
		case ActionType::Land: return landSheathed;
		case ActionType::Sneak: return sneakSheathed;
		case ActionType::UnSneak: return unSneakSheathed;
		case ActionType::TakingHit: return takingHitSheathed;
		case ActionType::Hitting: return hittingSheathed;
		case ActionType::ArrowRelease: return arrowReleaseSheathed;
		default: return walkForwardSheathed;
		}
	}
}

const ActionSettings& Settings::GetActionSettingsForState(ActionType a_type, bool a_weaponDrawn) const
{
	return const_cast<Settings*>(this)->GetActionSettingsForState(a_type, a_weaponDrawn);
}

