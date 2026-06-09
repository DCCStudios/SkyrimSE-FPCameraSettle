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
		"SneakWalkForward",
		"SneakWalkBackward",
		"SneakWalkLeft",
		"SneakWalkRight",
		"SneakRunForward",
		"SneakRunBackward",
		"SneakRunLeft",
		"SneakRunRight",
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

// === MovementNoiseSnapshot Load/Save ===
void MovementNoiseSnapshot::Load(CSimpleIniA& a_ini, const char* a_section)
{
	intensity      = static_cast<float>(a_ini.GetDoubleValue(a_section, "fIntensity", intensity));
	frequency      = static_cast<float>(a_ini.GetDoubleValue(a_section, "fFrequency", frequency));
	posAmpX        = static_cast<float>(a_ini.GetDoubleValue(a_section, "fPosAmpX", posAmpX));
	posAmpY        = static_cast<float>(a_ini.GetDoubleValue(a_section, "fPosAmpY", posAmpY));
	posAmpZ        = static_cast<float>(a_ini.GetDoubleValue(a_section, "fPosAmpZ", posAmpZ));
	rotAmpX        = static_cast<float>(a_ini.GetDoubleValue(a_section, "fRotAmpX", rotAmpX));
	rotAmpY        = static_cast<float>(a_ini.GetDoubleValue(a_section, "fRotAmpY", rotAmpY));
	rotAmpZ        = static_cast<float>(a_ini.GetDoubleValue(a_section, "fRotAmpZ", rotAmpZ));
	verticalBias   = static_cast<float>(a_ini.GetDoubleValue(a_section, "fVerticalBias", verticalBias));
	secondHarmonic = static_cast<float>(a_ini.GetDoubleValue(a_section, "fSecondHarmonic", secondHarmonic));
	lateralPhase   = static_cast<float>(a_ini.GetDoubleValue(a_section, "fLateralPhase", lateralPhase));
	blendIn        = static_cast<float>(a_ini.GetDoubleValue(a_section, "fBlendIn", blendIn));
	blendOut       = static_cast<float>(a_ini.GetDoubleValue(a_section, "fBlendOut", blendOut));
}

void MovementNoiseSnapshot::Save(CSimpleIniA& a_ini, const char* a_section) const
{
	a_ini.SetDoubleValue(a_section, "fIntensity", intensity);
	a_ini.SetDoubleValue(a_section, "fFrequency", frequency);
	a_ini.SetDoubleValue(a_section, "fPosAmpX", posAmpX);
	a_ini.SetDoubleValue(a_section, "fPosAmpY", posAmpY);
	a_ini.SetDoubleValue(a_section, "fPosAmpZ", posAmpZ);
	a_ini.SetDoubleValue(a_section, "fRotAmpX", rotAmpX);
	a_ini.SetDoubleValue(a_section, "fRotAmpY", rotAmpY);
	a_ini.SetDoubleValue(a_section, "fRotAmpZ", rotAmpZ);
	a_ini.SetDoubleValue(a_section, "fVerticalBias", verticalBias);
	a_ini.SetDoubleValue(a_section, "fSecondHarmonic", secondHarmonic);
	a_ini.SetDoubleValue(a_section, "fLateralPhase", lateralPhase);
	a_ini.SetDoubleValue(a_section, "fBlendIn", blendIn);
	a_ini.SetDoubleValue(a_section, "fBlendOut", blendOut);
}

// === MovementNoiseParams Load/Save ===
void MovementNoiseParams::Load(CSimpleIniA& a_ini, const char* a_section)
{
	enabled   = a_ini.GetBoolValue(a_section, "bEnabled", enabled);
	preset    = static_cast<int>(a_ini.GetLongValue(a_section, "iPreset", preset));
	preset    = std::clamp(preset, 0, 4);
	intensity = static_cast<float>(a_ini.GetDoubleValue(a_section, "fIntensity", intensity));
	intensity = std::clamp(intensity, 0.0f, 3.0f);
	frequency = static_cast<float>(a_ini.GetDoubleValue(a_section, "fFrequency", frequency));
	frequency = std::clamp(frequency, 0.5f, 10.0f);
	blendIn   = static_cast<float>(a_ini.GetDoubleValue(a_section, "fBlendIn", blendIn));
	blendIn   = std::clamp(blendIn, 0.05f, 2.0f);
	blendOut  = static_cast<float>(a_ini.GetDoubleValue(a_section, "fBlendOut", blendOut));
	blendOut  = std::clamp(blendOut, 0.05f, 5.0f);

	posAmpX = static_cast<float>(a_ini.GetDoubleValue(a_section, "fPosAmpX", posAmpX));
	posAmpY = static_cast<float>(a_ini.GetDoubleValue(a_section, "fPosAmpY", posAmpY));
	posAmpZ = static_cast<float>(a_ini.GetDoubleValue(a_section, "fPosAmpZ", posAmpZ));
	rotAmpX = static_cast<float>(a_ini.GetDoubleValue(a_section, "fRotAmpX", rotAmpX));
	rotAmpY = static_cast<float>(a_ini.GetDoubleValue(a_section, "fRotAmpY", rotAmpY));
	rotAmpZ = static_cast<float>(a_ini.GetDoubleValue(a_section, "fRotAmpZ", rotAmpZ));

	verticalBias   = static_cast<float>(a_ini.GetDoubleValue(a_section, "fVerticalBias", verticalBias));
	verticalBias   = std::clamp(verticalBias, 0.0f, 1.0f);
	secondHarmonic = static_cast<float>(a_ini.GetDoubleValue(a_section, "fSecondHarmonic", secondHarmonic));
	secondHarmonic = std::clamp(secondHarmonic, 0.0f, 1.0f);
	lateralPhase   = static_cast<float>(a_ini.GetDoubleValue(a_section, "fLateralPhase", lateralPhase));
	lateralPhase   = std::clamp(lateralPhase, 0.0f, 1.0f);

	// Load custom snapshot from companion section (e.g. "WalkNoise_Custom")
	std::string snapSection = std::string(a_section) + "_Custom";
	customSnapshot.Load(a_ini, snapSection.c_str());
}

void MovementNoiseParams::Save(CSimpleIniA& a_ini, const char* a_section) const
{
	a_ini.SetBoolValue(a_section, "bEnabled", enabled);
	a_ini.SetLongValue(a_section, "iPreset", preset);
	a_ini.SetDoubleValue(a_section, "fIntensity", intensity);
	a_ini.SetDoubleValue(a_section, "fFrequency", frequency);
	a_ini.SetDoubleValue(a_section, "fBlendIn", blendIn);
	a_ini.SetDoubleValue(a_section, "fBlendOut", blendOut);
	a_ini.SetDoubleValue(a_section, "fPosAmpX", posAmpX);
	a_ini.SetDoubleValue(a_section, "fPosAmpY", posAmpY);
	a_ini.SetDoubleValue(a_section, "fPosAmpZ", posAmpZ);
	a_ini.SetDoubleValue(a_section, "fRotAmpX", rotAmpX);
	a_ini.SetDoubleValue(a_section, "fRotAmpY", rotAmpY);
	a_ini.SetDoubleValue(a_section, "fRotAmpZ", rotAmpZ);
	a_ini.SetDoubleValue(a_section, "fVerticalBias", verticalBias);
	a_ini.SetDoubleValue(a_section, "fSecondHarmonic", secondHarmonic);
	a_ini.SetDoubleValue(a_section, "fLateralPhase", lateralPhase);

	std::string snapSection = std::string(a_section) + "_Custom";
	customSnapshot.Save(a_ini, snapSection.c_str());
}

void MovementNoiseParams::SaveToCustom()
{
	customSnapshot.intensity      = intensity;
	customSnapshot.frequency      = frequency;
	customSnapshot.posAmpX        = posAmpX;
	customSnapshot.posAmpY        = posAmpY;
	customSnapshot.posAmpZ        = posAmpZ;
	customSnapshot.rotAmpX        = rotAmpX;
	customSnapshot.rotAmpY        = rotAmpY;
	customSnapshot.rotAmpZ        = rotAmpZ;
	customSnapshot.verticalBias   = verticalBias;
	customSnapshot.secondHarmonic = secondHarmonic;
	customSnapshot.lateralPhase   = lateralPhase;
	customSnapshot.blendIn        = blendIn;
	customSnapshot.blendOut       = blendOut;
}

void MovementNoiseParams::LoadFromCustom()
{
	intensity      = customSnapshot.intensity;
	frequency      = customSnapshot.frequency;
	posAmpX        = customSnapshot.posAmpX;
	posAmpY        = customSnapshot.posAmpY;
	posAmpZ        = customSnapshot.posAmpZ;
	rotAmpX        = customSnapshot.rotAmpX;
	rotAmpY        = customSnapshot.rotAmpY;
	rotAmpZ        = customSnapshot.rotAmpZ;
	verticalBias   = customSnapshot.verticalBias;
	secondHarmonic = customSnapshot.secondHarmonic;
	lateralPhase   = customSnapshot.lateralPhase;
	blendIn        = customSnapshot.blendIn;
	blendOut       = customSnapshot.blendOut;
}

bool MovementNoiseParams::DiffersFromSnapshot() const
{
	constexpr float EPS = 0.0001f;
	return std::abs(intensity - customSnapshot.intensity) > EPS ||
	       std::abs(frequency - customSnapshot.frequency) > EPS ||
	       std::abs(posAmpX - customSnapshot.posAmpX) > EPS ||
	       std::abs(posAmpY - customSnapshot.posAmpY) > EPS ||
	       std::abs(posAmpZ - customSnapshot.posAmpZ) > EPS ||
	       std::abs(rotAmpX - customSnapshot.rotAmpX) > EPS ||
	       std::abs(rotAmpY - customSnapshot.rotAmpY) > EPS ||
	       std::abs(rotAmpZ - customSnapshot.rotAmpZ) > EPS ||
	       std::abs(verticalBias - customSnapshot.verticalBias) > EPS ||
	       std::abs(secondHarmonic - customSnapshot.secondHarmonic) > EPS ||
	       std::abs(lateralPhase - customSnapshot.lateralPhase) > EPS ||
	       std::abs(blendIn - customSnapshot.blendIn) > EPS ||
	       std::abs(blendOut - customSnapshot.blendOut) > EPS;
}

void MovementNoiseParams::CopyTunablesFrom(const MovementNoiseParams& other)
{
	intensity      = other.intensity;
	frequency      = other.frequency;
	posAmpX        = other.posAmpX;
	posAmpY        = other.posAmpY;
	posAmpZ        = other.posAmpZ;
	rotAmpX        = other.rotAmpX;
	rotAmpY        = other.rotAmpY;
	rotAmpZ        = other.rotAmpZ;
	verticalBias   = other.verticalBias;
	secondHarmonic = other.secondHarmonic;
	lateralPhase   = other.lateralPhase;
	blendIn        = other.blendIn;
	blendOut       = other.blendOut;
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
	
	// Sneak walk actions - even more subtle than normal walk
	auto initSneakWalk = [](ActionSettings& s, float xDir, float yDir) {
		s.enabled = true;
		s.multiplier = 1.0f;
		s.blendTime = 0.15f;  // Slower blend for stealth
		s.stiffness = 60.0f;
		s.damping = 7.0f;
		s.positionStrength = 1.5f;
		s.rotationStrength = 1.0f;
		s.impulseX = xDir * 2.0f;
		s.impulseY = yDir * 1.5f;
		s.impulseZ = 0.3f;
		s.rotImpulseX = yDir * 0.3f;
		s.rotImpulseY = xDir * 0.2f;
		s.rotImpulseZ = xDir * 0.5f;
	};
	
	// Sneak run actions - between walk and normal run
	auto initSneakRun = [](ActionSettings& s, float xDir, float yDir) {
		s.enabled = true;
		s.multiplier = 1.0f;
		s.blendTime = 0.1f;
		s.stiffness = 70.0f;
		s.damping = 6.5f;
		s.positionStrength = 2.5f;
		s.rotationStrength = 1.8f;
		s.impulseX = xDir * 3.5f;
		s.impulseY = yDir * 2.5f;
		s.impulseZ = 0.7f;
		s.rotImpulseX = yDir * 0.7f;
		s.rotImpulseY = xDir * 0.4f;
		s.rotImpulseZ = xDir * 1.0f;
	};
	
	// Initialize sneak walk settings (drawn)
	initSneakWalk(sneakWalkForwardDrawn, 0.0f, 1.0f);
	initSneakWalk(sneakWalkBackwardDrawn, 0.0f, -1.0f);
	initSneakWalk(sneakWalkLeftDrawn, -1.0f, 0.0f);
	initSneakWalk(sneakWalkRightDrawn, 1.0f, 0.0f);
	
	// Initialize sneak run settings (drawn)
	initSneakRun(sneakRunForwardDrawn, 0.0f, 1.0f);
	initSneakRun(sneakRunBackwardDrawn, 0.0f, -1.0f);
	initSneakRun(sneakRunLeftDrawn, -1.0f, 0.0f);
	initSneakRun(sneakRunRightDrawn, 1.0f, 0.0f);
	
	// Walk noise defaults (gentle breathing-like sway)
	walkNoise.enabled = false;
	walkNoise.preset = 1;
	walkNoise.intensity = 1.0f;
	walkNoise.frequency = 1.6f;
	walkNoise.posAmpX = 0.008f;
	walkNoise.posAmpY = 0.003f;
	walkNoise.posAmpZ = 0.015f;
	walkNoise.rotAmpX = 0.1f;
	walkNoise.rotAmpY = 0.08f;
	walkNoise.rotAmpZ = 0.04f;
	walkNoise.verticalBias = 0.3f;
	walkNoise.secondHarmonic = 0.1f;
	walkNoise.lateralPhase = 0.5f;
	walkNoise.blendIn = 0.5f;
	walkNoise.blendOut = 0.8f;
	walkNoise.SaveToCustom();
	
	// Run noise defaults (moderate footfall)
	runNoise.enabled = false;
	runNoise.preset = 1;
	runNoise.intensity = 1.0f;
	runNoise.frequency = 2.2f;
	runNoise.posAmpX = 0.02f;
	runNoise.posAmpY = 0.006f;
	runNoise.posAmpZ = 0.035f;
	runNoise.rotAmpX = 0.2f;
	runNoise.rotAmpY = 0.15f;
	runNoise.rotAmpZ = 0.08f;
	runNoise.verticalBias = 0.5f;
	runNoise.secondHarmonic = 0.2f;
	runNoise.lateralPhase = 0.5f;
	runNoise.blendIn = 0.4f;
	runNoise.blendOut = 0.7f;
	runNoise.SaveToCustom();
	
	// Sprint noise defaults (Natural preset values)
	sprintNoise.enabled = true;
	sprintNoise.preset = 1;
	sprintNoise.intensity = 1.0f;
	sprintNoise.frequency = 2.8f;
	sprintNoise.posAmpX = 0.04f;
	sprintNoise.posAmpY = 0.01f;
	sprintNoise.posAmpZ = 0.06f;
	sprintNoise.rotAmpX = 0.4f;
	sprintNoise.rotAmpY = 0.3f;
	sprintNoise.rotAmpZ = 0.15f;
	sprintNoise.verticalBias = 0.6f;
	sprintNoise.secondHarmonic = 0.3f;
	sprintNoise.lateralPhase = 0.5f;
	sprintNoise.blendIn = 0.3f;
	sprintNoise.blendOut = 0.8f;
	sprintNoise.SaveToCustom();
	
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
	sneakWalkForwardSheathed = sneakWalkForwardDrawn;
	sneakWalkBackwardSheathed = sneakWalkBackwardDrawn;
	sneakWalkLeftSheathed = sneakWalkLeftDrawn;
	sneakWalkRightSheathed = sneakWalkRightDrawn;
	sneakRunForwardSheathed = sneakRunForwardDrawn;
	sneakRunBackwardSheathed = sneakRunBackwardDrawn;
	sneakRunLeftSheathed = sneakRunLeftDrawn;
	sneakRunRightSheathed = sneakRunRightDrawn;
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
	
	// Load walk/run blending settings
	speedBasedBlending = ini.GetBoolValue("Movement", "bSpeedBasedBlending", speedBasedBlending);
	walkToRunGracePeriod = static_cast<float>(ini.GetDoubleValue("Movement", "fWalkToRunGracePeriod", walkToRunGracePeriod));
	
	// Load jump/land scaling settings
	scaleJumpByAirTime = ini.GetBoolValue("Jump", "bScaleByAirTime", scaleJumpByAirTime);
	jumpMinAirTime = static_cast<float>(ini.GetDoubleValue("Jump", "fMinAirTime", jumpMinAirTime));
	jumpMaxAirTimeScale = static_cast<float>(ini.GetDoubleValue("Jump", "fMaxAirTimeScale", jumpMaxAirTimeScale));
	landBaseScale = static_cast<float>(ini.GetDoubleValue("Jump", "fLandBaseScale", landBaseScale));
	landAirTimeScale = static_cast<float>(ini.GetDoubleValue("Jump", "fLandAirTimeScale", landAirTimeScale));
	
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
	
	// Load shared idle noise settings
	idleNoiseBlendTime = static_cast<float>(ini.GetDoubleValue("IdleNoise", "fBlendTime", idleNoiseBlendTime));
	dialogueDisableIdleNoise = ini.GetBoolValue("IdleNoise", "bDialogueDisableIdleNoise", dialogueDisableIdleNoise);
	idleNoiseScaleDuringArchery = ini.GetBoolValue("IdleNoise", "bScaleDuringArchery", idleNoiseScaleDuringArchery);
	idleNoiseArcheryScaleAmount = static_cast<float>(ini.GetDoubleValue("IdleNoise", "fArcheryScaleAmount", idleNoiseArcheryScaleAmount));
	idleNoiseArcheryScaleBySkill = ini.GetBoolValue("IdleNoise", "bArcheryScaleBySkill", idleNoiseArcheryScaleBySkill);
	idleNoiseArcheryScaleAmount = std::clamp(idleNoiseArcheryScaleAmount, 0.0f, 1.0f);
	idleNoiseEnabledSneaking = ini.GetBoolValue("IdleNoise", "bEnabledSneaking", idleNoiseEnabledSneaking);
	idleNoiseScaleSneaking = static_cast<float>(ini.GetDoubleValue("IdleNoise", "fScaleSneaking", idleNoiseScaleSneaking));
	idleNoiseScaleSneaking = std::clamp(idleNoiseScaleSneaking, 0.0f, 1.0f);
	
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

	// Load movement noise layers (walk, run, sprint)
	// Migration: if old flat [SprintNoise] has iStopMode but no [WalkNoise] section,
	// this is a pre-update INI — sprint values are read from [SprintNoise] as before,
	// walk/run get their InitializeDefaults() values.
	walkNoise.Load(ini, "WalkNoise");
	runNoise.Load(ini, "RunNoise");
	sprintNoise.Load(ini, "SprintNoise");
	sprintNoiseStopMode = static_cast<int>(ini.GetLongValue("SprintNoise", "iStopMode", sprintNoiseStopMode));
	sprintNoiseStopMode = std::clamp(sprintNoiseStopMode, 0, 2);

	// Load FOV punch settings
	fovPunchHitEnabled = ini.GetBoolValue("FOVPunch", "bHitEnabled", fovPunchHitEnabled);
	fovPunchArrowEnabled = ini.GetBoolValue("FOVPunch", "bArrowEnabled", fovPunchArrowEnabled);
	fovPunchHitStrength = static_cast<float>(ini.GetDoubleValue("FOVPunch", "fHitStrength", fovPunchHitStrength));
	fovPunchArrowStrength = static_cast<float>(ini.GetDoubleValue("FOVPunch", "fArrowStrength", fovPunchArrowStrength));
	fovPunchDuration = static_cast<float>(ini.GetDoubleValue("FOVPunch", "fDuration", fovPunchDuration));
	fovPunchHitStrength = std::clamp(fovPunchHitStrength, 0.0f, 20.0f);
	fovPunchArrowStrength = std::clamp(fovPunchArrowStrength, 0.0f, 20.0f);
	fovPunchDuration = std::clamp(fovPunchDuration, 0.05f, 1.0f);

	// Load Fall Effect settings
	fallEffectEnabled = ini.GetBoolValue("FallEffect", "bEnabled", fallEffectEnabled);
	fallTriggerTime = static_cast<float>(ini.GetDoubleValue("FallEffect", "fTriggerTime", fallTriggerTime));
	fallTriggerVelocity = static_cast<float>(ini.GetDoubleValue("FallEffect", "fTriggerVelocity", fallTriggerVelocity));
	fallRequireBothConditions = ini.GetBoolValue("FallEffect", "bRequireBothConditions", fallRequireBothConditions);
	fallPhase1Duration = static_cast<float>(ini.GetDoubleValue("FallEffect", "fPhase1Duration", fallPhase1Duration));
	fallPhase2Duration = static_cast<float>(ini.GetDoubleValue("FallEffect", "fPhase2Duration", fallPhase2Duration));

	fallShakeEnabled = ini.GetBoolValue("FallEffect_Shake", "bEnabled", fallShakeEnabled);
	fallShakeIntensity = static_cast<float>(ini.GetDoubleValue("FallEffect_Shake", "fIntensity", fallShakeIntensity));
	fallShakeFadeIn = static_cast<float>(ini.GetDoubleValue("FallEffect_Shake", "fFadeIn", fallShakeFadeIn));
	fallShakeFadeIn = std::clamp(fallShakeFadeIn, 0.0f, 10.0f);
	fallShakePosScale = static_cast<float>(ini.GetDoubleValue("FallEffect_Shake", "fPosScale", fallShakePosScale));
	fallShakeRotScale = static_cast<float>(ini.GetDoubleValue("FallEffect_Shake", "fRotScale", fallShakeRotScale));
	fallShakeFrequency = static_cast<float>(ini.GetDoubleValue("FallEffect_Shake", "fFrequency", fallShakeFrequency));
	fallShakeNoiseAmount = static_cast<float>(ini.GetDoubleValue("FallEffect_Shake", "fNoiseAmount", fallShakeNoiseAmount));
	fallShakeNoiseAmount = std::clamp(fallShakeNoiseAmount, 0.0f, 1.0f);
	fallShakeAffectPosition = ini.GetBoolValue("FallEffect_Shake", "bAffectPosition", fallShakeAffectPosition);
	fallShakeAffectRoll = ini.GetBoolValue("FallEffect_Shake", "bAffectRoll", fallShakeAffectRoll);
	fallShakeAffectPitch = ini.GetBoolValue("FallEffect_Shake", "bAffectPitch", fallShakeAffectPitch);
	fallShakeAffectYaw = ini.GetBoolValue("FallEffect_Shake", "bAffectYaw", fallShakeAffectYaw);
	fallShakeDownwardBias = static_cast<float>(ini.GetDoubleValue("FallEffect_Shake", "fDownwardBias", fallShakeDownwardBias));
	fallShakeScaleByVelocity = ini.GetBoolValue("FallEffect_Shake", "bScaleByVelocity", fallShakeScaleByVelocity);

	fallAudioEnabled = ini.GetBoolValue("FallEffect_Audio", "bEnabled", fallAudioEnabled);
	fallWindEnabled = ini.GetBoolValue("FallEffect_Audio", "bWindEnabled", fallWindEnabled);
	fallWhineEnabled = ini.GetBoolValue("FallEffect_Audio", "bWhineEnabled", fallWhineEnabled);
	fallMasterVolume = static_cast<float>(ini.GetDoubleValue("FallEffect_Audio", "fMasterVolume", fallMasterVolume));
	fallWindMaxVolume = static_cast<float>(ini.GetDoubleValue("FallEffect_Audio", "fWindMaxVolume", fallWindMaxVolume));
	fallWhineMaxVolume = static_cast<float>(ini.GetDoubleValue("FallEffect_Audio", "fWhineMaxVolume", fallWhineMaxVolume));
	fallWindFadeIn   = static_cast<float>(ini.GetDoubleValue("FallEffect_Audio", "fWindFadeIn",   fallWindFadeIn));
	fallWhineFadeIn  = static_cast<float>(ini.GetDoubleValue("FallEffect_Audio", "fWhineFadeIn",  fallWhineFadeIn));
	fallAudioFadeOut = static_cast<float>(ini.GetDoubleValue("FallEffect_Audio", "fFadeOut", fallAudioFadeOut));
	fallAudioVolumeByVelocity = ini.GetBoolValue("FallEffect_Audio", "bVolumeByVelocity", fallAudioVolumeByVelocity);
	fallFadeCurve = static_cast<int>(ini.GetLongValue("FallEffect_Audio", "iFadeCurve", fallFadeCurve));
	fallMasterVolume   = std::clamp(fallMasterVolume,   0.0f, 5.0f);
	fallWindMaxVolume  = std::clamp(fallWindMaxVolume,  0.0f, 5.0f);
	fallWhineMaxVolume = std::clamp(fallWhineMaxVolume, 0.0f, 5.0f);
	fallWindFadeIn     = std::clamp(fallWindFadeIn,     0.0f, 10.0f);
	fallWhineFadeIn    = std::clamp(fallWhineFadeIn,    0.0f, 10.0f);
	fallAudioFadeOut   = std::clamp(fallAudioFadeOut,   0.05f, 5.0f);
	fallFadeCurve      = std::clamp(fallFadeCurve,      0, 4);

	fallDoubleVisionEnabled = ini.GetBoolValue("FallEffect_Visual", "bDoubleVisionEnabled", fallDoubleVisionEnabled);
	fallDoubleVisionMaxStrength = static_cast<float>(ini.GetDoubleValue("FallEffect_Visual", "fDoubleVisionMaxStrength", fallDoubleVisionMaxStrength));
	fallDoubleVisionFadeIn = static_cast<float>(ini.GetDoubleValue("FallEffect_Visual", "fDoubleVisionFadeIn", fallDoubleVisionFadeIn));
	fallMotionBlurEnabled = ini.GetBoolValue("FallEffect_Visual", "bMotionBlurEnabled", fallMotionBlurEnabled);
	fallMotionBlurMaxStrength = static_cast<float>(ini.GetDoubleValue("FallEffect_Visual", "fMotionBlurMaxStrength", fallMotionBlurMaxStrength));
	fallMotionBlurFadeIn = static_cast<float>(ini.GetDoubleValue("FallEffect_Visual", "fMotionBlurFadeIn", fallMotionBlurFadeIn));
	fallDoubleVisionMaxStrength = std::clamp(fallDoubleVisionMaxStrength, 0.0f, 2.0f);
	fallDoubleVisionFadeIn = std::clamp(fallDoubleVisionFadeIn, 0.0f, 10.0f);
	fallMotionBlurMaxStrength = std::clamp(fallMotionBlurMaxStrength, 0.0f, 2.0f);
	fallMotionBlurFadeIn = std::clamp(fallMotionBlurFadeIn, 0.0f, 10.0f);

	fallFovEnabled = ini.GetBoolValue("FallEffect_FOV", "bEnabled", fallFovEnabled);
	fallFovOscAmplitude = static_cast<float>(ini.GetDoubleValue("FallEffect_FOV", "fOscAmplitude", fallFovOscAmplitude));
	fallFovOscFrequency = static_cast<float>(ini.GetDoubleValue("FallEffect_FOV", "fOscFrequency", fallFovOscFrequency));

	// Fatal landing
	fallFatalEnabled       = ini.GetBoolValue("FallEffect_Fatal", "bEnabled", fallFatalEnabled);
	fallFatalBlackDuration = static_cast<float>(ini.GetDoubleValue("FallEffect_Fatal", "fBlackDuration", fallFatalBlackDuration));
	fallFatalFadeInTime    = static_cast<float>(ini.GetDoubleValue("FallEffect_Fatal", "fFadeInTime", fallFatalFadeInTime));
	fallFatalFadeOutTime   = static_cast<float>(ini.GetDoubleValue("FallEffect_Fatal", "fFadeOutTime", fallFatalFadeOutTime));
	fallFatalWhineBoost    = static_cast<float>(ini.GetDoubleValue("FallEffect_Fatal", "fWhineBoost", fallFatalWhineBoost));
	fallFatalWhineDecay    = static_cast<float>(ini.GetDoubleValue("FallEffect_Fatal", "fWhineDecay", fallFatalWhineDecay));
	fallFatalImpactVolume  = static_cast<float>(ini.GetDoubleValue("FallEffect_Fatal", "fImpactVolume", fallFatalImpactVolume));
	fallFatalBlackDuration = std::clamp(fallFatalBlackDuration, 0.1f, 10.0f);
	fallFatalFadeInTime    = std::clamp(fallFatalFadeInTime,    0.01f, 2.0f);
	fallFatalFadeOutTime   = std::clamp(fallFatalFadeOutTime,   0.1f, 5.0f);
	fallFatalWhineBoost    = std::clamp(fallFatalWhineBoost,    0.0f, 10.0f);
	fallFatalWhineDecay    = std::clamp(fallFatalWhineDecay,    0.1f, 5.0f);
	fallFatalImpactVolume  = std::clamp(fallFatalImpactVolume,  0.0f, 5.0f);

	// Load lean settings
	leanEnabled = ini.GetBoolValue("Lean", "bEnabled", leanEnabled);
	leanIntensity = static_cast<float>(ini.GetDoubleValue("Lean", "fIntensity", leanIntensity));
	leanIntensity = std::clamp(leanIntensity, 0.0f, 5.0f);

	leanManualEnabled = ini.GetBoolValue("Lean", "bManualEnabled", leanManualEnabled);
	leanManualMode = static_cast<int>(ini.GetLongValue("Lean", "iManualMode", leanManualMode));
	leanManualMode = std::clamp(leanManualMode, 0, 1);
	leanLeftScancode = static_cast<int>(ini.GetLongValue("Lean", "iLeftScancode", leanLeftScancode));
	leanRightScancode = static_cast<int>(ini.GetLongValue("Lean", "iRightScancode", leanRightScancode));

	leanContextualEnabled = ini.GetBoolValue("Lean_Contextual", "bEnabled", leanContextualEnabled);
	leanContextualGamepadOnly = ini.GetBoolValue("Lean_Contextual", "bGamepadOnly", leanContextualGamepadOnly);
	leanContextualDistance = static_cast<float>(ini.GetDoubleValue("Lean_Contextual", "fDistance", leanContextualDistance));
	leanContextualDistance = std::clamp(leanContextualDistance, 10.0f, 500.0f);
	leanContextualOffset = static_cast<float>(ini.GetDoubleValue("Lean_Contextual", "fOffset", leanContextualOffset));
	leanContextualOffset = std::clamp(leanContextualOffset, 5.0f, 100.0f);
	leanContextualDeadzone = static_cast<float>(ini.GetDoubleValue("Lean_Contextual", "fDeadzone", leanContextualDeadzone));
	leanContextualDeadzone = std::clamp(leanContextualDeadzone, 0.0f, 0.5f);
	leanContextualBow = ini.GetBoolValue("Lean_Contextual", "bBow", leanContextualBow);
	leanContextualCrossbow = ini.GetBoolValue("Lean_Contextual", "bCrossbow", leanContextualCrossbow);
	leanContextualMagic = ini.GetBoolValue("Lean_Contextual", "bMagic", leanContextualMagic);
	leanContextualHoldTime = static_cast<float>(ini.GetDoubleValue("Lean_Contextual", "fHoldTime",
		ini.GetDoubleValue("Lean_Contextual", "fMagicHoldTime", leanContextualHoldTime)));
	leanContextualHoldTime = std::clamp(leanContextualHoldTime, 0.0f, 3.0f);
	leanMagicUseHandOrigin = ini.GetBoolValue("Lean_Contextual", "bMagicUseHandOrigin", leanMagicUseHandOrigin);

	leanPosAmount = static_cast<float>(ini.GetDoubleValue("Lean_Camera", "fPosAmount", leanPosAmount));
	leanPosAmount = std::clamp(leanPosAmount, 0.0f, 30.0f);
	leanRollDegrees = static_cast<float>(ini.GetDoubleValue("Lean_Camera", "fRollDegrees", leanRollDegrees));
	leanRollDegrees = std::clamp(leanRollDegrees, 0.0f, 30.0f);
	leanYawDegrees = static_cast<float>(ini.GetDoubleValue("Lean_Camera", "fYawDegrees", leanYawDegrees));
	leanYawDegrees = std::clamp(leanYawDegrees, 0.0f, 15.0f);
	leanForwardAmount = static_cast<float>(ini.GetDoubleValue("Lean_Camera", "fForwardAmount", leanForwardAmount));
	leanForwardAmount = std::clamp(leanForwardAmount, 0.0f, 20.0f);

	leanBlendSpeed = static_cast<float>(ini.GetDoubleValue("Lean", "fBlendSpeed", leanBlendSpeed));
	leanBlendSpeed = std::clamp(leanBlendSpeed, 1.0f, 20.0f);
	leanReturnSpeed = static_cast<float>(ini.GetDoubleValue("Lean", "fReturnSpeed", leanReturnSpeed));
	leanReturnSpeed = std::clamp(leanReturnSpeed, 1.0f, 20.0f);

	leanFirstPersonEnabled = ini.GetBoolValue("Lean_1P", "bEnabled", leanFirstPersonEnabled);
	leanFirstPersonScale = static_cast<float>(ini.GetDoubleValue("Lean_1P", "fScale", leanFirstPersonScale));
	leanFirstPersonScale = std::clamp(leanFirstPersonScale, 0.0f, 3.0f);
	leanFirstPersonNode = static_cast<int>(ini.GetLongValue("Lean_1P", "iNode", leanFirstPersonNode));
	leanFirstPersonNode = std::clamp(leanFirstPersonNode, 0, 2);

	leanThirdPersonEnabled = ini.GetBoolValue("Lean_3P", "bEnabled", leanThirdPersonEnabled);
	leanThirdPersonScale = static_cast<float>(ini.GetDoubleValue("Lean_3P", "fScale", leanThirdPersonScale));
	leanThirdPersonScale = std::clamp(leanThirdPersonScale, 0.0f, 3.0f);

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
	sneakWalkForwardDrawn.Load(ini, "SneakWalkForward_Drawn");
	sneakWalkBackwardDrawn.Load(ini, "SneakWalkBackward_Drawn");
	sneakWalkLeftDrawn.Load(ini, "SneakWalkLeft_Drawn");
	sneakWalkRightDrawn.Load(ini, "SneakWalkRight_Drawn");
	sneakRunForwardDrawn.Load(ini, "SneakRunForward_Drawn");
	sneakRunBackwardDrawn.Load(ini, "SneakRunBackward_Drawn");
	sneakRunLeftDrawn.Load(ini, "SneakRunLeft_Drawn");
	sneakRunRightDrawn.Load(ini, "SneakRunRight_Drawn");
	
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
	sneakWalkForwardSheathed.Load(ini, "SneakWalkForward_Sheathed");
	sneakWalkBackwardSheathed.Load(ini, "SneakWalkBackward_Sheathed");
	sneakWalkLeftSheathed.Load(ini, "SneakWalkLeft_Sheathed");
	sneakWalkRightSheathed.Load(ini, "SneakWalkRight_Sheathed");
	sneakRunForwardSheathed.Load(ini, "SneakRunForward_Sheathed");
	sneakRunBackwardSheathed.Load(ini, "SneakRunBackward_Sheathed");
	sneakRunLeftSheathed.Load(ini, "SneakRunLeft_Sheathed");
	sneakRunRightSheathed.Load(ini, "SneakRunRight_Sheathed");
	
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
	
	// Movement settings
	ini.SetBoolValue("Movement", "bSpeedBasedBlending", speedBasedBlending, "; Blend walk/run impulse based on actual speed instead of binary toggle");
	ini.SetDoubleValue("Movement", "fWalkToRunGracePeriod", walkToRunGracePeriod, "; Skip walk impulse if player reaches run speed within this time (seconds)");
	
	// Jump/land scaling settings
	ini.SetBoolValue("Jump", "bScaleByAirTime", scaleJumpByAirTime, "; Scale landing impulse based on air time (also prevents jump impulse when walking off ledges)");
	ini.SetDoubleValue("Jump", "fMinAirTime", jumpMinAirTime, "; Minimum air time to trigger landing impulse (ignores short drops)");
	ini.SetDoubleValue("Jump", "fMaxAirTimeScale", jumpMaxAirTimeScale, "; Air time above this is capped for scaling purposes");
	ini.SetDoubleValue("Jump", "fLandBaseScale", landBaseScale, "; Base landing impulse scale (always applied above min air time)");
	ini.SetDoubleValue("Jump", "fLandAirTimeScale", landAirTimeScale, "; Additional scale from air time (0 to this based on air time)");
	
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
	
	// Shared idle noise settings
	ini.SetDoubleValue("IdleNoise", "fBlendTime", idleNoiseBlendTime, "; Blend in/out time in seconds");
	ini.SetBoolValue("IdleNoise", "bDialogueDisableIdleNoise", dialogueDisableIdleNoise, "; Disable idle camera noise in dialogue and map menus (blends out smoothly)");
	ini.SetBoolValue("IdleNoise", "bScaleDuringArchery", idleNoiseScaleDuringArchery, "; Scale idle noise down while drawing bow/crossbow");
	ini.SetDoubleValue("IdleNoise", "fArcheryScaleAmount", idleNoiseArcheryScaleAmount, "; Scale amount while drawing (0-1, e.g., 0.1 = 10%)");
	ini.SetBoolValue("IdleNoise", "bArcheryScaleBySkill", idleNoiseArcheryScaleBySkill, "; Scale amount based on Archery skill (100 = 0)");
	ini.SetBoolValue("IdleNoise", "bEnabledSneaking", idleNoiseEnabledSneaking, "; Allow idle camera noise while sneaking and standing still");
	ini.SetDoubleValue("IdleNoise", "fScaleSneaking", idleNoiseScaleSneaking, "; Scale multiplier for idle noise while sneaking (0-1, e.g., 0.5 = 50%)");
	
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

	// Movement noise layers (walk, run, sprint) + custom snapshots
	walkNoise.Save(ini, "WalkNoise");
	runNoise.Save(ini, "RunNoise");
	sprintNoise.Save(ini, "SprintNoise");
	ini.SetLongValue("SprintNoise", "iStopMode", sprintNoiseStopMode, "; Stop detection: 0=Sprint State, 1=Input Release, 2=Speed-Based");

	// FOV punch settings
	ini.SetBoolValue("FOVPunch", "bHitEnabled", fovPunchHitEnabled, "; Enable FOV punch when taking a hit");
	ini.SetBoolValue("FOVPunch", "bArrowEnabled", fovPunchArrowEnabled, "; Enable FOV punch on arrow/bolt release");
	ini.SetDoubleValue("FOVPunch", "fHitStrength", fovPunchHitStrength, "; Hit punch strength as percent of current FOV (e.g., 5.0 = +/-5%)");
	ini.SetDoubleValue("FOVPunch", "fArrowStrength", fovPunchArrowStrength, "; Arrow punch strength as percent of current FOV (e.g., 3.0 = +/-3%)");
	ini.SetDoubleValue("FOVPunch", "fDuration", fovPunchDuration, "; Total punch duration in seconds");

	// Fall Effect (Mirror's Edge style disorientation)
	ini.SetBoolValue("FallEffect", "bEnabled", fallEffectEnabled, "; Master toggle for the entire fall disorientation effect");
	ini.SetDoubleValue("FallEffect", "fTriggerTime", fallTriggerTime, "; Time in seconds in the air before the effect can begin");
	ini.SetDoubleValue("FallEffect", "fTriggerVelocity", fallTriggerVelocity, "; Downward velocity threshold (units/sec) to trigger");
	ini.SetBoolValue("FallEffect", "bRequireBothConditions", fallRequireBothConditions, "; If true, BOTH air time AND velocity must be exceeded");
	ini.SetDoubleValue("FallEffect", "fPhase1Duration", fallPhase1Duration, "; Phase 1 -> Phase 2 transition time (seconds since fall start)");
	ini.SetDoubleValue("FallEffect", "fPhase2Duration", fallPhase2Duration, "; Phase 2 length (Phase 3 begins at fPhase1Duration + fPhase2Duration)");

	ini.SetBoolValue("FallEffect_Shake", "bEnabled", fallShakeEnabled, "; Enable procedural camera shake during falling");
	ini.SetDoubleValue("FallEffect_Shake", "fIntensity", fallShakeIntensity, "; Master shake intensity multiplier (0-3)");
	ini.SetDoubleValue("FallEffect_Shake", "fFadeIn", fallShakeFadeIn, "; Time from fall start to full shake intensity (seconds)");
	ini.SetDoubleValue("FallEffect_Shake", "fPosScale", fallShakePosScale, "; Position shake scale (0-3)");
	ini.SetDoubleValue("FallEffect_Shake", "fRotScale", fallShakeRotScale, "; Rotation shake scale (0-3)");
	ini.SetDoubleValue("FallEffect_Shake", "fFrequency", fallShakeFrequency, "; Base shake oscillation frequency (Hz)");
	ini.SetDoubleValue("FallEffect_Shake", "fNoiseAmount", fallShakeNoiseAmount, "; Mix between sine (0) and noise (1)");
	ini.SetBoolValue("FallEffect_Shake", "bAffectPosition", fallShakeAffectPosition, "; Apply shake to camera position");
	ini.SetBoolValue("FallEffect_Shake", "bAffectRoll", fallShakeAffectRoll, "; Allow roll component");
	ini.SetBoolValue("FallEffect_Shake", "bAffectPitch", fallShakeAffectPitch, "; Allow pitch component");
	ini.SetBoolValue("FallEffect_Shake", "bAffectYaw", fallShakeAffectYaw, "; Allow yaw component");
	ini.SetDoubleValue("FallEffect_Shake", "fDownwardBias", fallShakeDownwardBias, "; Downward pitch bias added in Phase 3 (degrees)");
	ini.SetBoolValue("FallEffect_Shake", "bScaleByVelocity", fallShakeScaleByVelocity, "; Scale shake intensity by fall velocity");

	ini.SetBoolValue("FallEffect_Audio", "bEnabled", fallAudioEnabled, "; Enable audio (wind + whine)");
	ini.SetBoolValue("FallEffect_Audio", "bWindEnabled", fallWindEnabled, "; Play wind loop");
	ini.SetBoolValue("FallEffect_Audio", "bWhineEnabled", fallWhineEnabled, "; Play whine loop");
	ini.SetDoubleValue("FallEffect_Audio", "fMasterVolume",  fallMasterVolume,  "; Master volume for all fall audio (0-5; >1.0 amplifies WAV)");
	ini.SetDoubleValue("FallEffect_Audio", "fWindMaxVolume", fallWindMaxVolume, "; Wind loop maximum volume (0-5; >1.0 amplifies WAV)");
	ini.SetDoubleValue("FallEffect_Audio", "fWhineMaxVolume",fallWhineMaxVolume,"; Whine loop maximum volume (0-5; >1.0 amplifies WAV)");
	ini.SetDoubleValue("FallEffect_Audio", "fWindFadeIn",    fallWindFadeIn,    "; Wind fade-in time from start of fall (seconds)");
	ini.SetDoubleValue("FallEffect_Audio", "fWhineFadeIn",   fallWhineFadeIn,   "; Whine fade-in time from start of Phase 2 (seconds)");
	ini.SetDoubleValue("FallEffect_Audio", "fFadeOut",       fallAudioFadeOut,  "; Audio fade-out time on landing (seconds)");
	ini.SetBoolValue("FallEffect_Audio", "bVolumeByVelocity", fallAudioVolumeByVelocity, "; Scale audio volume by fall velocity");
	ini.SetLongValue("FallEffect_Audio", "iFadeCurve", fallFadeCurve, "; Fade curve: 0=Linear, 1=Smooth, 2=EaseIn, 3=EaseOut, 4=Exponential");

	ini.SetBoolValue("FallEffect_Visual", "bDoubleVisionEnabled", fallDoubleVisionEnabled, "; Enable double-vision IMOD overlay");
	ini.SetDoubleValue("FallEffect_Visual", "fDoubleVisionMaxStrength", fallDoubleVisionMaxStrength, "; Double vision maximum strength (0-2)");
	ini.SetDoubleValue("FallEffect_Visual", "fDoubleVisionFadeIn", fallDoubleVisionFadeIn, "; Time from Phase 2 start to full DV strength (seconds)");
	ini.SetBoolValue("FallEffect_Visual", "bMotionBlurEnabled", fallMotionBlurEnabled, "; Enable motion / radial blur");
	ini.SetDoubleValue("FallEffect_Visual", "fMotionBlurMaxStrength", fallMotionBlurMaxStrength, "; Motion blur maximum strength (0-2)");
	ini.SetDoubleValue("FallEffect_Visual", "fMotionBlurFadeIn", fallMotionBlurFadeIn, "; Time from Phase 3 start to full blur strength (seconds)");

	ini.SetBoolValue("FallEffect_FOV", "bEnabled", fallFovEnabled, "; Enable FOV oscillation in Phase 3");
	ini.SetDoubleValue("FallEffect_FOV", "fOscAmplitude", fallFovOscAmplitude, "; FOV oscillation amplitude (degrees)");
	ini.SetDoubleValue("FallEffect_FOV", "fOscFrequency", fallFovOscFrequency, "; FOV oscillation frequency (Hz)");

	ini.SetBoolValue("FallEffect_Fatal", "bEnabled", fallFatalEnabled, "; Enable Mirror's Edge-style fatal landing (cut to black on death by fall)");
	ini.SetDoubleValue("FallEffect_Fatal", "fBlackDuration", fallFatalBlackDuration, "; How long the black screen is held (seconds)");
	ini.SetDoubleValue("FallEffect_Fatal", "fFadeInTime", fallFatalFadeInTime, "; How fast the screen goes black on impact (seconds, very fast)");
	ini.SetDoubleValue("FallEffect_Fatal", "fFadeOutTime", fallFatalFadeOutTime, "; How fast the screen fades back from black (seconds)");
	ini.SetDoubleValue("FallEffect_Fatal", "fWhineBoost", fallFatalWhineBoost, "; Whine volume spike on impact (multiplier of configured max)");
	ini.SetDoubleValue("FallEffect_Fatal", "fWhineDecay", fallFatalWhineDecay, "; How long the whine spike takes to decay (seconds)");
	ini.SetDoubleValue("FallEffect_Fatal", "fImpactVolume", fallFatalImpactVolume, "; Impact body-slam sound volume (0-5; >1.0 amplifies)");

	// Lean settings
	ini.SetBoolValue("Lean", "bEnabled", leanEnabled, "; Master toggle for leaning system");
	ini.SetDoubleValue("Lean", "fIntensity", leanIntensity, "; Master lean intensity multiplier");
	ini.SetBoolValue("Lean", "bManualEnabled", leanManualEnabled, "; Enable manual lean via keyboard");
	ini.SetLongValue("Lean", "iManualMode", leanManualMode, "; 0=Hold, 1=Toggle");
	ini.SetLongValue("Lean", "iLeftScancode", leanLeftScancode, "; Lean left key scancode (0x10 = Q)");
	ini.SetLongValue("Lean", "iRightScancode", leanRightScancode, "; Lean right key scancode (0x12 = E)");
	ini.SetDoubleValue("Lean", "fBlendSpeed", leanBlendSpeed, "; How fast lean blends in");
	ini.SetDoubleValue("Lean", "fReturnSpeed", leanReturnSpeed, "; How fast lean returns to center");

	ini.SetBoolValue("Lean_Contextual", "bEnabled", leanContextualEnabled, "; Enable automatic lean near walls during ranged combat");
	ini.SetBoolValue("Lean_Contextual", "bGamepadOnly", leanContextualGamepadOnly, "; Only activate contextual lean when using a gamepad");
	ini.SetDoubleValue("Lean_Contextual", "fDistance", leanContextualDistance, "; Max raycast distance for wall detection");
	ini.SetDoubleValue("Lean_Contextual", "fOffset", leanContextualOffset, "; Shoulder offset for ray origins");
	ini.SetDoubleValue("Lean_Contextual", "fDeadzone", leanContextualDeadzone, "; Minimum proximity ratio to trigger lean");
	ini.SetBoolValue("Lean_Contextual", "bBow", leanContextualBow, "; Contextual lean while drawing bow");
	ini.SetBoolValue("Lean_Contextual", "bCrossbow", leanContextualCrossbow, "; Contextual lean while aiming crossbow");
	ini.SetBoolValue("Lean_Contextual", "bMagic", leanContextualMagic, "; Contextual lean while casting spells");
	ini.SetDoubleValue("Lean_Contextual", "fHoldTime", leanContextualHoldTime, "; Seconds to hold lean after firing/casting ends");
	ini.SetBoolValue("Lean_Contextual", "bMagicUseHandOrigin", leanMagicUseHandOrigin, "; Spawn spells from actual hand node position");

	ini.SetDoubleValue("Lean_Camera", "fPosAmount", leanPosAmount, "; Lateral camera shift (units)");
	ini.SetDoubleValue("Lean_Camera", "fRollDegrees", leanRollDegrees, "; Head tilt roll (degrees)");
	ini.SetDoubleValue("Lean_Camera", "fYawDegrees", leanYawDegrees, "; Look-around yaw (degrees)");
	ini.SetDoubleValue("Lean_Camera", "fForwardAmount", leanForwardAmount, "; Forward peek (units)");

	ini.SetBoolValue("Lean_1P", "bEnabled", leanFirstPersonEnabled, "; Apply lean to first-person skeleton");
	ini.SetDoubleValue("Lean_1P", "fScale", leanFirstPersonScale, "; First-person lean scale");
	ini.SetLongValue("Lean_1P", "iNode", leanFirstPersonNode, "; Spine node: 0=Spine, 1=Spine1, 2=Spine2");

	ini.SetBoolValue("Lean_3P", "bEnabled", leanThirdPersonEnabled, "; Apply lean to third-person body");
	ini.SetDoubleValue("Lean_3P", "fScale", leanThirdPersonScale, "; Third-person lean scale");

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
	sneakWalkForwardDrawn.Save(ini, "SneakWalkForward_Drawn");
	sneakWalkBackwardDrawn.Save(ini, "SneakWalkBackward_Drawn");
	sneakWalkLeftDrawn.Save(ini, "SneakWalkLeft_Drawn");
	sneakWalkRightDrawn.Save(ini, "SneakWalkRight_Drawn");
	sneakRunForwardDrawn.Save(ini, "SneakRunForward_Drawn");
	sneakRunBackwardDrawn.Save(ini, "SneakRunBackward_Drawn");
	sneakRunLeftDrawn.Save(ini, "SneakRunLeft_Drawn");
	sneakRunRightDrawn.Save(ini, "SneakRunRight_Drawn");
	
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
	sneakWalkForwardSheathed.Save(ini, "SneakWalkForward_Sheathed");
	sneakWalkBackwardSheathed.Save(ini, "SneakWalkBackward_Sheathed");
	sneakWalkLeftSheathed.Save(ini, "SneakWalkLeft_Sheathed");
	sneakWalkRightSheathed.Save(ini, "SneakWalkRight_Sheathed");
	sneakRunForwardSheathed.Save(ini, "SneakRunForward_Sheathed");
	sneakRunBackwardSheathed.Save(ini, "SneakRunBackward_Sheathed");
	sneakRunLeftSheathed.Save(ini, "SneakRunLeft_Sheathed");
	sneakRunRightSheathed.Save(ini, "SneakRunRight_Sheathed");
	
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
		case ActionType::SneakWalkForward: return sneakWalkForwardDrawn;
		case ActionType::SneakWalkBackward: return sneakWalkBackwardDrawn;
		case ActionType::SneakWalkLeft: return sneakWalkLeftDrawn;
		case ActionType::SneakWalkRight: return sneakWalkRightDrawn;
		case ActionType::SneakRunForward: return sneakRunForwardDrawn;
		case ActionType::SneakRunBackward: return sneakRunBackwardDrawn;
		case ActionType::SneakRunLeft: return sneakRunLeftDrawn;
		case ActionType::SneakRunRight: return sneakRunRightDrawn;
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
		case ActionType::SneakWalkForward: return sneakWalkForwardSheathed;
		case ActionType::SneakWalkBackward: return sneakWalkBackwardSheathed;
		case ActionType::SneakWalkLeft: return sneakWalkLeftSheathed;
		case ActionType::SneakWalkRight: return sneakWalkRightSheathed;
		case ActionType::SneakRunForward: return sneakRunForwardSheathed;
		case ActionType::SneakRunBackward: return sneakRunBackwardSheathed;
		case ActionType::SneakRunLeft: return sneakRunLeftSheathed;
		case ActionType::SneakRunRight: return sneakRunRightSheathed;
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

