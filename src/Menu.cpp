#include "Menu.h"
#include "CameraSettle.h"
#include "FallEffect.h"
#include "Lean.h"
#include <Windows.h>
#include <Xinput.h>

#pragma comment(lib, "xinput.lib")

namespace Menu
{
	// -------------------------------------------------------------------
	// Win32 key polling for keybind capture (BSInputDeviceManager doesn't
	// fire when SKSE Menu Framework is active)
	// -------------------------------------------------------------------
	static bool s_prevKeyState[256] = {};
	static bool s_prevMouseState[5] = {};
	static WORD s_prevGamepadButtons = 0;

	// Maps XInput button mask to our gamepad encoding (266+)
	static int XInputButtonToEncoded(WORD a_button)
	{
		static const std::pair<WORD, int> mapping[] = {
			{ XINPUT_GAMEPAD_DPAD_UP,        266 },
			{ XINPUT_GAMEPAD_DPAD_DOWN,       267 },
			{ XINPUT_GAMEPAD_DPAD_LEFT,       268 },
			{ XINPUT_GAMEPAD_DPAD_RIGHT,      269 },
			{ XINPUT_GAMEPAD_START,            270 },
			{ XINPUT_GAMEPAD_BACK,             271 },
			{ XINPUT_GAMEPAD_LEFT_THUMB,       272 },
			{ XINPUT_GAMEPAD_RIGHT_THUMB,      273 },
			{ XINPUT_GAMEPAD_LEFT_SHOULDER,    274 },
			{ XINPUT_GAMEPAD_RIGHT_SHOULDER,   275 },
			{ XINPUT_GAMEPAD_A,                276 },
			{ XINPUT_GAMEPAD_B,                277 },
			{ XINPUT_GAMEPAD_X,                278 },
			{ XINPUT_GAMEPAD_Y,                279 },
		};
		for (auto& [mask, enc] : mapping) {
			if (a_button & mask) return enc;
		}
		return -1;
	}

	// VK to DirectInput scancode mapping for common keys
	static uint32_t VKToDIScancode(int a_vk)
	{
		UINT sc = MapVirtualKeyA(static_cast<UINT>(a_vk), MAPVK_VK_TO_VSC);
		return (sc > 0 && sc < 256) ? sc : 0;
	}

	// Poll Win32 for a newly-pressed key/button. Returns encoded scancode or -1.
	static int PollForNewKeyPress()
	{
		// Escape check first
		bool escNow = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
		if (escNow && !s_prevKeyState[VK_ESCAPE]) {
			s_prevKeyState[VK_ESCAPE] = true;
			return 0x01;  // DI scancode for Escape
		}
		s_prevKeyState[VK_ESCAPE] = escNow;

		// Keyboard (skip mouse VKs 1-6 and modifier keys that might be held)
		for (int vk = 0x08; vk < 256; ++vk) {
			if (vk == VK_ESCAPE) continue;
			bool now = (GetAsyncKeyState(vk) & 0x8000) != 0;
			if (now && !s_prevKeyState[vk]) {
				s_prevKeyState[vk] = true;
				uint32_t sc = VKToDIScancode(vk);
				if (sc > 0) return static_cast<int>(sc);
			}
			s_prevKeyState[vk] = now;
		}

		// Mouse buttons
		static const int mouseVKs[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
		for (int i = 0; i < 5; ++i) {
			bool now = (GetAsyncKeyState(mouseVKs[i]) & 0x8000) != 0;
			if (now && !s_prevMouseState[i]) {
				s_prevMouseState[i] = true;
				return 256 + i;
			}
			s_prevMouseState[i] = now;
		}

		// Gamepad
		XINPUT_STATE xstate{};
		if (XInputGetState(0, &xstate) == ERROR_SUCCESS) {
			WORD buttons = xstate.Gamepad.wButtons;
			WORD newButtons = buttons & ~s_prevGamepadButtons;
			s_prevGamepadButtons = buttons;
			if (newButtons) {
				int enc = XInputButtonToEncoded(newButtons);
				if (enc >= 0) return enc;
			}
		} else {
			s_prevGamepadButtons = 0;
		}

		return -1;
	}

	// -------------------------------------------------------------------
	// Key name lookup for encoded scancodes
	// Keyboard: 0-255, Mouse: 256-265, Gamepad: 266+
	// -------------------------------------------------------------------
	const char* GetKeyName(int a_encoded)
	{
		if (a_encoded < 0) return "None";

		// Mouse buttons (256+)
		if (a_encoded >= 256 && a_encoded < 266) {
			static const char* mouseNames[] = {
				"Mouse Left", "Mouse Right", "Mouse Middle",
				"Mouse 4", "Mouse 5", "Mouse 6",
				"Mouse 7", "Mouse 8", "Scroll Up", "Scroll Down"
			};
			int idx = a_encoded - 256;
			if (idx < 10) return mouseNames[idx];
			static char mbuf[32];
			snprintf(mbuf, sizeof(mbuf), "Mouse %d", idx);
			return mbuf;
		}

		// Gamepad buttons (266+)
		if (a_encoded >= 266) {
			static const char* gpNames[] = {
				"Gamepad Up", "Gamepad Down", "Gamepad Left", "Gamepad Right",
				"Gamepad Start", "Gamepad Back",
				"Gamepad L Thumb", "Gamepad R Thumb",
				"Gamepad LB", "Gamepad RB",
				"Gamepad A", "Gamepad B", "Gamepad X", "Gamepad Y"
			};
			int idx = a_encoded - 266;
			if (idx < 14) return gpNames[idx];
			static char gbuf[32];
			snprintf(gbuf, sizeof(gbuf), "Gamepad %d", idx);
			return gbuf;
		}

		// Keyboard scancodes (0-255)
		static const char* keyNames[] = {
			"None",        "Escape",     "1",          "2",          "3",          "4",          "5",          "6",         // 0x00-0x07
			"7",           "8",          "9",          "0",          "-",          "=",          "Backspace",  "Tab",       // 0x08-0x0F
			"Q",           "W",          "E",          "R",          "T",          "Y",          "U",          "I",         // 0x10-0x17
			"O",           "P",          "[",          "]",          "Enter",      "Left Ctrl",  "A",          "S",         // 0x18-0x1F
			"D",           "F",          "G",          "H",          "J",          "K",          "L",          ";",         // 0x20-0x27
			"'",           "`",          "Left Shift", "\\",         "Z",          "X",          "C",          "V",         // 0x28-0x2F
			"B",           "N",          "M",          ",",          ".",          "/",          "Right Shift","Num *",     // 0x30-0x37
			"Left Alt",    "Space",      "Caps Lock",  "F1",         "F2",         "F3",         "F4",         "F5",        // 0x38-0x3F
			"F6",          "F7",         "F8",         "F9",         "F10",        "Num Lock",   "Scroll Lock","Num 7",     // 0x40-0x47
			"Num 8",       "Num 9",      "Num -",      "Num 4",      "Num 5",      "Num 6",      "Num +",      "Num 1",     // 0x48-0x4F
			"Num 2",       "Num 3",      "Num 0",      "Num .",      nullptr,      nullptr,      nullptr,      "F11",       // 0x50-0x57
			"F12"                                                                                                            // 0x58
		};
		constexpr int numKeys = sizeof(keyNames) / sizeof(keyNames[0]);

		if (a_encoded < numKeys && keyNames[a_encoded]) {
			return keyNames[a_encoded];
		}

		// Extended keys
		switch (a_encoded) {
		case 0x9C: return "Num Enter";
		case 0x9D: return "Right Ctrl";
		case 0xB5: return "Num /";
		case 0xB8: return "Right Alt";
		case 0xC7: return "Home";
		case 0xC8: return "Up Arrow";
		case 0xC9: return "Page Up";
		case 0xCB: return "Left Arrow";
		case 0xCD: return "Right Arrow";
		case 0xCF: return "End";
		case 0xD0: return "Down Arrow";
		case 0xD1: return "Page Down";
		case 0xD2: return "Insert";
		case 0xD3: return "Delete";
		default: break;
		}

		static char kbuf[32];
		snprintf(kbuf, sizeof(kbuf), "Key 0x%02X", a_encoded);
		return kbuf;
	}

	const char* CheckKeyConflict(int a_encoded)
	{
		auto* controlMap = RE::ControlMap::GetSingleton();
		if (!controlMap) return nullptr;

		RE::INPUT_DEVICE device = RE::INPUT_DEVICE::kKeyboard;
		uint32_t idCode = static_cast<uint32_t>(a_encoded);
		if (a_encoded >= 266) {
			device = RE::INPUT_DEVICE::kGamepad;
			idCode = a_encoded - 266;
		} else if (a_encoded >= 256) {
			device = RE::INPUT_DEVICE::kMouse;
			idCode = a_encoded - 256;
		}

		auto* userEvents = RE::UserEvents::GetSingleton();
		if (!userEvents) return nullptr;

		struct ActionCheck { const RE::BSFixedString* eventName; const char* displayName; };
		ActionCheck checks[] = {
			{ &userEvents->forward,       "Forward" },
			{ &userEvents->back,          "Back" },
			{ &userEvents->strafeLeft,    "Strafe Left" },
			{ &userEvents->strafeRight,   "Strafe Right" },
			{ &userEvents->activate,      "Activate" },
			{ &userEvents->jump,          "Jump" },
			{ &userEvents->sprint,        "Sprint" },
			{ &userEvents->sneak,         "Sneak" },
			{ &userEvents->readyWeapon,   "Ready Weapon" },
			{ &userEvents->rightAttack,   "Right Attack" },
			{ &userEvents->leftAttack,    "Left Attack" },
			{ &userEvents->shout,         "Shout" },
			{ &userEvents->togglePOV,     "Toggle POV" },
		};

		for (auto& check : checks) {
			if (!check.eventName || check.eventName->empty()) continue;
			uint32_t mapped = controlMap->GetMappedKey(*check.eventName, device);
			if (mapped == idCode) {
				return check.displayName;
			}
		}

		return nullptr;
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::warn("SKSE Menu Framework is not installed - menu will not be available");
			return;
		}
		
		SKSEMenuFramework::SetSection("FP Camera Settle");
		SKSEMenuFramework::AddSectionItem("Settings", Render);
		
		logger::info("Menu registered with SKSE Menu Framework");
	}
	
	bool SliderFloatWithTooltip(const char* label, float* value, float min, float max, const char* format, const char* tooltip)
	{
		bool changed = ImGui::SliderFloat(label, value, min, max, format);
		if (ImGui::IsItemHovered() && tooltip && tooltip[0]) {
			ImGui::SetTooltip("%s", tooltip);
		}
		return changed;
	}
	
	bool CheckboxWithTooltip(const char* label, bool* value, const char* tooltip)
	{
		bool changed = ImGui::Checkbox(label, value);
		if (ImGui::IsItemHovered() && tooltip && tooltip[0]) {
			ImGui::SetTooltip("%s", tooltip);
		}
		return changed;
	}
	
	bool SliderIntWithTooltip(const char* label, int* value, int min, int max, const char* format, const char* tooltip)
	{
		bool changed = ImGui::SliderInt(label, value, min, max, format);
		if (ImGui::IsItemHovered() && tooltip && tooltip[0]) {
			ImGui::SetTooltip("%s", tooltip);
		}
		return changed;
	}
	
	void __stdcall Render()
	{
		if (!State::initialized) {
			State::initialized = true;
			State::hasUnsavedChanges = false;
		}
		
		DrawHeader();
		ImGui::Separator();
		
		// Disable all settings when not in Edit Mode
		if (!State::editMode) {
			ImGui::BeginDisabled();
		}
		
		DrawGeneralSettings();
		DrawWeaponStateSettings();
		DrawMovementSettings();
		DrawJumpSettings();
		DrawSettlingSettings();
		DrawIdleNoiseSettings();
		DrawSprintEffectsSettings();
		DrawMovementNoiseSettings();
		DrawFovPunchSettings();
		DrawFallEffectSettings();
		DrawLeanSettings();
		DrawDebugSettings();
		
		ImGui::Separator();
		DrawActionSettings();
		
		if (!State::editMode) {
			ImGui::EndDisabled();
			
			// Show message about edit mode
			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
			ImGui::TextWrapped("Enable Edit Mode above to modify settings. Changes will apply instantly while playing.");
			ImGui::PopStyleColor();
		}
		
		ImGui::Separator();
		DrawSaveLoadButtons();
	}
	
	void DrawHeader()
	{
		auto* settings = Settings::GetSingleton();
		
		// Edit Mode toggle - prominent at the top
		ImGui::PushStyleColor(ImGuiCol_Text, State::editMode ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
		if (ImGui::Checkbox("Edit Mode", &State::editMode)) {
			settings->SetEditMode(State::editMode);
		}
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("When enabled, changes apply instantly while playing.\nDisable for better performance when not editing.");
		}
		
		ImGui::SameLine();
		ImGui::Text("|");
		ImGui::SameLine();
		
		// Enabled toggle
		if (ImGui::Checkbox("Enabled", &settings->enabled)) {
			MarkSettingsChanged();
		}
		
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "FP Camera Settle v1.0.0");
		
		// Second row: status indicators
		// Show first-person indicator
		auto* camera = RE::PlayerCamera::GetSingleton();
		bool inFirstPerson = camera && camera->IsInFirstPerson();
		
		if (inFirstPerson) {
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[1st Person]");
		} else {
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[3rd Person]");
		}
		
		// Show weapon drawn state
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (player) {
			auto* actorState = player->AsActorState();
			bool weaponDrawn = actorState && actorState->IsWeaponDrawn();
			
			ImGui::SameLine();
			if (weaponDrawn) {
				ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Weapon Drawn]");
			} else {
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[Sheathed]");
			}
		}
		
		// Unsaved changes indicator
		if (State::hasUnsavedChanges) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "(Unsaved changes)");
		}
	}
	
	void DrawGeneralSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("General Settings", State::generalExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::generalExpanded = true;
			
			if (SliderFloatWithTooltip("Global Intensity", &settings->globalIntensity, 0.0f, 5.0f, "%.2f",
				"Master multiplier for all camera settle effects")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Smoothing Factor", &settings->smoothingFactor, 0.0f, 1.0f, "%.2f",
				"Input smoothing (0 = no smoothing, 1 = maximum)")) {
				MarkSettingsChanged();
			}
			
			ImGui::Spacing();
			
			if (CheckboxWithTooltip("Disable on Pause", &settings->resetOnPause,
				"Disable camera effects when the game is paused (menus, console, etc.).\n\n"
				"When enabled, opening any menu will reset and disable camera offsets,\n"
				"preventing jarring jumps when you close the menu.")) {
				MarkSettingsChanged();
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Performance:");
			
			if (SliderIntWithTooltip("Spring Substeps", &settings->springSubsteps, 1, 8, "%d",
				"Number of physics sub-steps per frame.\n\n"
				"Higher values = more stable/accurate spring physics\n"
				"Lower values = better performance\n\n"
				"1-2: Fast, may be jittery with large movements\n"
				"3-4: Balanced (recommended)\n"
				"5-8: Very stable, higher CPU cost")) {
				settings->springSubsteps = std::clamp(settings->springSubsteps, 1, 8);
				MarkSettingsChanged();
			}
		} else {
			State::generalExpanded = false;
		}
	}
	
	void DrawWeaponStateSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("Weapon State Settings", State::weaponStateExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::weaponStateExpanded = true;
			
			ImGui::TextWrapped("Configure different intensities for weapon drawn vs sheathed.");
			ImGui::Spacing();
			
			if (CheckboxWithTooltip("Enable When Drawn", &settings->weaponDrawnEnabled,
				"Enable camera settle effects when weapon is drawn")) {
				MarkSettingsChanged();
			}
			
			if (settings->weaponDrawnEnabled) {
				if (SliderFloatWithTooltip("Drawn Multiplier", &settings->weaponDrawnMult, 0.0f, 5.0f, "%.2f",
					"Effect intensity multiplier when weapon is drawn")) {
					MarkSettingsChanged();
				}
			}
			
			ImGui::Spacing();
			
			if (CheckboxWithTooltip("Enable When Sheathed", &settings->weaponSheathedEnabled,
				"Enable camera settle effects when weapon is sheathed")) {
				MarkSettingsChanged();
			}
			
			if (settings->weaponSheathedEnabled) {
				if (SliderFloatWithTooltip("Sheathed Multiplier", &settings->weaponSheathedMult, 0.0f, 5.0f, "%.2f",
					"Effect intensity multiplier when weapon is sheathed")) {
					MarkSettingsChanged();
				}
			}
		} else {
			State::weaponStateExpanded = false;
		}
	}
	
	void DrawMovementSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("Movement Settings", State::movementExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::movementExpanded = true;
			
			ImGui::TextWrapped("Controls walk/run blending and impulse behavior.");
			ImGui::Spacing();
			
			if (CheckboxWithTooltip("Speed-Based Blending", &settings->speedBasedBlending,
				"Blend walk/run impulses based on actual controller input magnitude.\n\n"
				"When enabled: Analog sticks will smoothly blend between walk and run\n"
				"When disabled: Binary walk/run based on toggle key only")) {
				MarkSettingsChanged();
			}
			
			ImGui::Spacing();
			
			if (SliderFloatWithTooltip("Walk-to-Run Grace Period", &settings->walkToRunGracePeriod, 0.0f, 0.5f, "%.2f sec",
				"If player goes from stationary to running within this time,\n"
				"the walk impulse is skipped.\n\n"
				"Prevents jarring walk impulse when you intend to immediately sprint/run.\n"
				"Set to 0 to disable (always trigger walk impulse).")) {
				MarkSettingsChanged();
			}
		} else {
			State::movementExpanded = false;
		}
	}
	
	void DrawJumpSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("Jump/Land Settings", State::jumpExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::jumpExpanded = true;
			
			ImGui::TextWrapped("Controls jump detection and landing impulse scaling.");
			ImGui::Spacing();
			
			if (CheckboxWithTooltip("Scale by Air Time", &settings->scaleJumpByAirTime,
				"Scale landing impulse based on how long you were in the air.\n\n"
				"Also prevents jump impulse when walking off ledges\n"
				"(only actual jumps trigger the jump impulse).")) {
				MarkSettingsChanged();
			}
			
			if (settings->scaleJumpByAirTime) {
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Text("Air Time Thresholds:");
				
				if (SliderFloatWithTooltip("Min Air Time", &settings->jumpMinAirTime, 0.0f, 0.5f, "%.2f sec",
					"Minimum air time to trigger any landing impulse.\n\n"
					"Drops shorter than this are ignored (stairs, small bumps).\n"
					"0.1-0.15 = good for most cases")) {
					MarkSettingsChanged();
				}
				
				if (SliderFloatWithTooltip("Max Air Time Scale", &settings->jumpMaxAirTimeScale, 0.5f, 5.0f, "%.1f sec",
					"Air time above this is capped for scaling purposes.\n\n"
					"Higher = longer falls can have bigger impacts")) {
					MarkSettingsChanged();
				}
				
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Text("Landing Impulse Scale:");
				
				if (SliderFloatWithTooltip("Base Scale", &settings->landBaseScale, 0.0f, 1.0f, "%.2f",
					"Base landing impulse scale (always applied above min air time).\n\n"
					"0.3 = 30% of configured landing impulse for minimum falls")) {
					MarkSettingsChanged();
				}
				
				if (SliderFloatWithTooltip("Air Time Scale", &settings->landAirTimeScale, 0.0f, 2.0f, "%.2f",
					"Additional scale based on air time (added to base).\n\n"
					"At max air time: total scale = Base + this value\n"
					"0.7 = adds up to 70% more based on fall duration")) {
					MarkSettingsChanged();
				}
			}
		} else {
			State::jumpExpanded = false;
		}
	}
	
	void DrawSettlingSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("Settling Behavior", State::settlingExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::settlingExpanded = true;
			
			ImGui::TextWrapped("Controls how the spring dampens over time when no actions are occurring.");
			ImGui::Spacing();
			
			if (SliderFloatWithTooltip("Settle Delay", &settings->settleDelay, 0.0f, 2.0f, "%.2f sec",
				"Delay before extra settling damping kicks in")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Settle Speed", &settings->settleSpeed, 0.5f, 10.0f, "%.1f",
				"How fast the extra damping increases")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Settle Damping Mult", &settings->settleDampingMult, 1.0f, 10.0f, "%.1fx",
				"Maximum damping multiplier when fully settled")) {
				MarkSettingsChanged();
			}
		} else {
			State::settlingExpanded = false;
		}
	}
	
	void DrawIdleNoiseSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("Idle Camera Noise", State::idleNoiseExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::idleNoiseExpanded = true;
			
			ImGui::TextWrapped("Subtle breathing/sway motion when standing idle. Separate settings for weapon drawn vs sheathed.");
			ImGui::Spacing();
			
			ImGui::BeginDisabled(!State::editMode);
			
			// Shared blend time setting
			if (SliderFloatWithTooltip("Blend Time", &settings->idleNoiseBlendTime, 0.05f, 1.0f, "%.2f sec",
				"How long to blend in/out the idle noise when transitioning\n"
				"Lower = faster transition\n"
				"Higher = smoother, slower transition")) {
				MarkSettingsChanged();
			}
			
			// Dialogue/Map disable option
			if (CheckboxWithTooltip("Disable in Menus", &settings->dialogueDisableIdleNoise,
				"Disable idle camera noise when in dialogue or map menu.\n\n"
				"When enabled, the idle noise will smoothly blend out\n"
				"when entering these menus and blend back in after leaving.")) {
				MarkSettingsChanged();
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Archery Scale:");
			
			if (CheckboxWithTooltip("Scale While Drawing Bow", &settings->idleNoiseScaleDuringArchery,
				"Smoothly scale idle noise down while drawing a bow or crossbow,\n"
				"then scale back up after release.")) {
				MarkSettingsChanged();
			}
			
			ImGui::BeginDisabled(!settings->idleNoiseScaleDuringArchery || settings->idleNoiseArcheryScaleBySkill);
			if (SliderFloatWithTooltip("Draw Scale Amount", &settings->idleNoiseArcheryScaleAmount, 0.0f, 1.0f, "%.2f",
				"Idle noise scale while drawing (0 = none, 1 = full).\n"
				"Default 0.10 = 10% of normal noise.")) {
				MarkSettingsChanged();
			}
			ImGui::EndDisabled();
			
			ImGui::BeginDisabled(!settings->idleNoiseScaleDuringArchery);
			if (CheckboxWithTooltip("Scale by Archery Skill", &settings->idleNoiseArcheryScaleBySkill,
				"When enabled, the scale amount is based on Archery skill.\n"
				"100 Archery = 0 (no idle noise while drawing).")) {
				MarkSettingsChanged();
			}
			ImGui::EndDisabled();
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Sneaking:");
			
			if (CheckboxWithTooltip("Allow While Sneaking", &settings->idleNoiseEnabledSneaking,
				"When enabled, idle camera noise can play while you are sneaking\n"
				"and standing still (same idle rules as normal).\n\n"
				"When disabled, idle noise is fully blocked while sneaking.")) {
				MarkSettingsChanged();
			}
			
			ImGui::BeginDisabled(!settings->idleNoiseEnabledSneaking);
			if (SliderFloatWithTooltip("Sneak Scale", &settings->idleNoiseScaleSneaking, 0.0f, 1.0f, "%.2f",
				"Multiplier for idle noise amplitude while sneaking (0 = none, 1 = full).\n"
				"Ramps smoothly when entering or leaving sneak.")) {
				MarkSettingsChanged();
			}
			ImGui::EndDisabled();
			
			ImGui::Spacing();
			
			// === WEAPON DRAWN ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.2f, 0.2f, 0.6f));
			if (ImGui::TreeNode("Weapon Drawn##IdleNoise")) {
				ImGui::PopStyleColor();
				
				if (CheckboxWithTooltip("Enabled##IdleDrawn", &settings->idleNoiseEnabledDrawn,
					"Enable idle camera noise when weapon is drawn")) {
					MarkSettingsChanged();
				}
				
				if (settings->idleNoiseEnabledDrawn) {
					ImGui::Separator();
					ImGui::Text("Position Amplitude:");
					
					if (SliderFloatWithTooltip("X (Left/Right)##IdleDrawn", &settings->idleNoisePosAmpXDrawn, 0.0f, 0.5f, "%.3f",
						"Side-to-side position noise amplitude")) {
						MarkSettingsChanged();
					}
					if (SliderFloatWithTooltip("Y (Forward/Back)##IdleDrawn", &settings->idleNoisePosAmpYDrawn, 0.0f, 0.5f, "%.3f",
						"Forward/backward position noise amplitude")) {
						MarkSettingsChanged();
					}
					if (SliderFloatWithTooltip("Z (Up/Down)##IdleDrawn", &settings->idleNoisePosAmpZDrawn, 0.0f, 0.5f, "%.3f",
						"Up/down position noise amplitude (breathing)")) {
						MarkSettingsChanged();
					}
					
					ImGui::Separator();
					ImGui::Text("Rotation Amplitude (degrees):");
					
					if (SliderFloatWithTooltip("Pitch##IdleDrawn", &settings->idleNoiseRotAmpXDrawn, 0.0f, 2.0f, "%.2f",
						"Head pitch noise amplitude")) {
						MarkSettingsChanged();
					}
					if (SliderFloatWithTooltip("Roll##IdleDrawn", &settings->idleNoiseRotAmpYDrawn, 0.0f, 2.0f, "%.2f",
						"Head roll noise amplitude")) {
						MarkSettingsChanged();
					}
					if (SliderFloatWithTooltip("Yaw##IdleDrawn", &settings->idleNoiseRotAmpZDrawn, 0.0f, 2.0f, "%.2f",
						"Head yaw noise amplitude")) {
						MarkSettingsChanged();
					}
					
					ImGui::Separator();
					if (SliderFloatWithTooltip("Frequency##IdleDrawn", &settings->idleNoiseFrequencyDrawn, 0.1f, 1.0f, "%.2f",
						"Noise frequency (cycles per second). Lower = slower, more relaxed")) {
						MarkSettingsChanged();
					}
				}
				
				ImGui::TreePop();
			} else {
				ImGui::PopStyleColor();
			}
			
			ImGui::Spacing();
			
			// === WEAPON SHEATHED ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.3f, 0.4f, 0.6f));
			if (ImGui::TreeNode("Weapon Sheathed##IdleNoise")) {
				ImGui::PopStyleColor();
				
				if (CheckboxWithTooltip("Enabled##IdleSheathed", &settings->idleNoiseEnabledSheathed,
					"Enable idle camera noise when weapon is sheathed")) {
					MarkSettingsChanged();
				}
				
				if (settings->idleNoiseEnabledSheathed) {
					ImGui::Separator();
					ImGui::Text("Position Amplitude:");
					
					if (SliderFloatWithTooltip("X (Left/Right)##IdleSheathed", &settings->idleNoisePosAmpXSheathed, 0.0f, 0.5f, "%.3f",
						"Side-to-side position noise amplitude")) {
						MarkSettingsChanged();
					}
					if (SliderFloatWithTooltip("Y (Forward/Back)##IdleSheathed", &settings->idleNoisePosAmpYSheathed, 0.0f, 0.5f, "%.3f",
						"Forward/backward position noise amplitude")) {
						MarkSettingsChanged();
					}
					if (SliderFloatWithTooltip("Z (Up/Down)##IdleSheathed", &settings->idleNoisePosAmpZSheathed, 0.0f, 0.5f, "%.3f",
						"Up/down position noise amplitude (breathing)")) {
						MarkSettingsChanged();
					}
					
					ImGui::Separator();
					ImGui::Text("Rotation Amplitude (degrees):");
					
					if (SliderFloatWithTooltip("Pitch##IdleSheathed", &settings->idleNoiseRotAmpXSheathed, 0.0f, 2.0f, "%.2f",
						"Head pitch noise amplitude")) {
						MarkSettingsChanged();
					}
					if (SliderFloatWithTooltip("Roll##IdleSheathed", &settings->idleNoiseRotAmpYSheathed, 0.0f, 2.0f, "%.2f",
						"Head roll noise amplitude")) {
						MarkSettingsChanged();
					}
					if (SliderFloatWithTooltip("Yaw##IdleSheathed", &settings->idleNoiseRotAmpZSheathed, 0.0f, 2.0f, "%.2f",
						"Head yaw noise amplitude")) {
						MarkSettingsChanged();
					}
					
					ImGui::Separator();
					if (SliderFloatWithTooltip("Frequency##IdleSheathed", &settings->idleNoiseFrequencySheathed, 0.1f, 1.0f, "%.2f",
						"Noise frequency (cycles per second). Lower = slower, more relaxed")) {
						MarkSettingsChanged();
					}
				}
				
				ImGui::TreePop();
			} else {
				ImGui::PopStyleColor();
			}
			
			ImGui::EndDisabled();
		} else {
			State::idleNoiseExpanded = false;
		}
	}
	
	void DrawSprintEffectsSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("Sprint Effects", State::sprintEffectsExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::sprintEffectsExpanded = true;
			
			ImGui::TextWrapped("Visual effects applied when sprinting: FOV increase and radial blur.");
			ImGui::Spacing();
			
			ImGui::BeginDisabled(!State::editMode);
			
			// === FOV SETTINGS ===
			ImGui::Separator();
			ImGui::Text("Field of View:");
			
			if (CheckboxWithTooltip("Enable FOV Effect", &settings->sprintFovEnabled,
				"Increase FOV when sprinting for a sense of speed")) {
				MarkSettingsChanged();
			}
			
			if (settings->sprintFovEnabled) {
				if (SliderFloatWithTooltip("FOV Delta", &settings->sprintFovDelta, 0.0f, 30.0f, "+%.1f degrees",
					"Amount to increase FOV when sprinting\n(added to current first-person FOV)")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Blend Speed##FOV", &settings->sprintFovBlendSpeed, 0.5f, 10.0f, "%.1f",
					"How fast to blend in/out the FOV change\n(higher = faster transition)")) {
					MarkSettingsChanged();
				}
			}
			
			ImGui::Spacing();
			
			// === BLUR SETTINGS ===
			ImGui::Separator();
			ImGui::Text("Radial Blur:");
			
			if (CheckboxWithTooltip("Enable Radial Blur", &settings->sprintBlurEnabled,
				"Apply radial blur effect when sprinting")) {
				MarkSettingsChanged();
			}
			
			if (settings->sprintBlurEnabled) {
				if (SliderFloatWithTooltip("Blur Strength", &settings->sprintBlurStrength, 0.0f, 3.0f, "%.2f",
					"Intensity of the radial blur effect\n(0 = none, 1 = normal, 3 = intense)")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Blend Speed##Blur", &settings->sprintBlurBlendSpeed, 0.5f, 10.0f, "%.1f",
					"How fast the blur strength transitions\n(higher = faster blend in/out)")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Ramp Up Time", &settings->sprintBlurRampUp, 0.0f, 0.5f, "%.2f sec",
					"How quickly the blur effect ramps up when triggered\n"
					"Lower = snappier blur appearance\n"
					"Higher = gradual blur fade-in")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Ramp Down Time", &settings->sprintBlurRampDown, 0.0f, 0.5f, "%.2f sec",
					"How quickly the blur effect fades when stopping\n"
					"Lower = snappier blur disappearance\n"
					"Higher = lingering blur fade-out")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Center Clarity", &settings->sprintBlurRadius, 0.0f, 1.0f, "%.2f",
					"How much of the screen center stays unblurred\n"
					"0 = blur starts from center (full blur)\n"
					"0.5 = center half stays clear\n"
					"1 = only edges are blurred")) {
					MarkSettingsChanged();
				}
			}
			
			ImGui::EndDisabled();
		} else {
			State::sprintEffectsExpanded = false;
		}
	}

	// Layer type enum for preset application
	enum class NoiseLayer { Walk = 0, Run, Sprint };

	static void ApplyMovementNoisePreset(MovementNoiseParams& params, int preset, NoiseLayer layer)
	{
		if (preset == 0) {
			params.LoadFromCustom();
			return;
		}

		// Scale factors relative to sprint (sprint = 1.0)
		struct LayerScale { float freq; float pos; float rot; float bias; float h2; };
		static const LayerScale scales[] = {
			{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },  // placeholder
			{ 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },  // sprint
		};

		// Base values per preset (sprint reference)
		struct PresetBase {
			float freq, posX, posY, posZ, rotX, rotY, rotZ, bias, h2, lat;
		};
		static const PresetBase presets[] = {
			{},  // 0 = Custom (handled above)
			{ 2.8f, 0.04f,  0.01f,  0.06f,  0.4f,  0.3f,  0.15f, 0.6f, 0.3f,  0.5f },  // Natural
			{ 3.0f, 0.015f, 0.005f, 0.025f, 0.15f, 0.1f,  0.06f, 0.3f, 0.15f, 0.5f },  // Subtle
			{ 2.5f, 0.06f,  0.02f,  0.09f,  0.7f,  0.5f,  0.25f, 0.7f, 0.4f,  0.5f },  // Cinematic
			{ 2.2f, 0.08f,  0.03f,  0.12f,  1.0f,  0.7f,  0.35f, 0.8f, 0.5f,  0.5f },  // Heavy
		};

		// Per-layer multipliers relative to sprint preset values
		struct LayerMult { float freq; float pos; float rot; float bias; float h2; };
		static const LayerMult layerMult[] = {
			{ 0.571f, 0.25f, 0.25f, 0.5f, 0.333f },  // Walk  (e.g. 1.6/2.8, 0.008/0.04 etc.)
			{ 0.786f, 0.50f, 0.50f, 0.833f, 0.667f }, // Run
			{ 1.0f,   1.0f,  1.0f,  1.0f,  1.0f },    // Sprint
		};

		int li = static_cast<int>(layer);
		const auto& p = presets[preset];
		const auto& m = layerMult[li];

		params.frequency      = p.freq * m.freq;
		params.posAmpX        = p.posX * m.pos;
		params.posAmpY        = p.posY * m.pos;
		params.posAmpZ        = p.posZ * m.pos;
		params.rotAmpX        = p.rotX * m.rot;
		params.rotAmpY        = p.rotY * m.rot;
		params.rotAmpZ        = p.rotZ * m.rot;
		params.verticalBias   = p.bias * m.bias;
		params.secondHarmonic = p.h2   * m.h2;
		params.lateralPhase   = p.lat;
	}

	// Draw one movement noise subsection (walk, run, or sprint)
	static void DrawNoiseLayerUI(const char* label, MovementNoiseParams& params, NoiseLayer layer,
	                              Settings* settings, bool showStopMode)
	{
		const char* presetNames[] = { "Custom", "Natural", "Subtle", "Cinematic", "Heavy" };

		// Build the preset display label with unsaved indicator
		char presetLabel[32];
		if (params.preset == 0 && params.DiffersFromSnapshot()) {
			snprintf(presetLabel, sizeof(presetLabel), "Custom*");
		} else {
			snprintf(presetLabel, sizeof(presetLabel), "%s", presetNames[params.preset]);
		}

		if (CheckboxWithTooltip(fmt::format("Enable##{}", label).c_str(), &params.enabled,
			"Enable rhythmic camera noise for this movement type.")) {
			MarkSettingsChanged();
		}

		ImGui::Spacing();

		// Preset row
		ImGui::Text("Preset:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150.0f);

		// Custom combo with asterisk support
		if (ImGui::BeginCombo(fmt::format("##Preset{}", label).c_str(), presetLabel)) {
			for (int i = 0; i < 5; ++i) {
				const char* displayName = presetNames[i];
				char itemLabel[32];
				if (i == 0 && params.DiffersFromSnapshot()) {
					snprintf(itemLabel, sizeof(itemLabel), "Custom*");
					displayName = itemLabel;
				}
				bool isSelected = (params.preset == i);
				if (ImGui::Selectable(displayName, isSelected)) {
					params.preset = i;
					ApplyMovementNoisePreset(params, i, layer);
					MarkSettingsChanged();
				}
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"Select a preset or use Custom for manual settings.\n\n"
				"Natural: Realistic motion (default)\n"
				"Subtle: Barely perceptible\n"
				"Cinematic: Pronounced movement\n"
				"Heavy: Strong, weighty motion\n"
				"Custom: Your saved custom values\n"
				"Custom*: Unsaved changes (use Save to Custom)");
		}

		ImGui::SameLine();
		if (ImGui::Button(fmt::format("Save to Custom##{}", label).c_str())) {
			params.SaveToCustom();
			params.preset = 0;
			MarkSettingsChanged();
			Settings::GetSingleton()->Save();
			State::hasUnsavedChanges = false;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Save current slider values as your Custom preset.\n"
				"These values persist across sessions.");
		}

		ImGui::Spacing();

		// Global sliders
		if (SliderFloatWithTooltip(fmt::format("Intensity##{}", label).c_str(), &params.intensity, 0.0f, 3.0f, "%.2f",
			"Master intensity multiplier.")) {
			params.preset = 0;
			MarkSettingsChanged();
		}
		if (SliderFloatWithTooltip(fmt::format("Frequency##{}", label).c_str(), &params.frequency, 0.5f, 10.0f, "%.1f Hz",
			"Rhythm frequency (footfall cadence).")) {
			params.preset = 0;
			MarkSettingsChanged();
		}
		if (SliderFloatWithTooltip(fmt::format("Blend In##{}", label).c_str(), &params.blendIn, 0.05f, 2.0f, "%.2f sec",
			"How long to ramp the noise in.")) {
			MarkSettingsChanged();
		}
		if (SliderFloatWithTooltip(fmt::format("Blend Out##{}", label).c_str(), &params.blendOut, 0.05f, 5.0f, "%.2f sec",
			"How long to fade the noise out.")) {
			MarkSettingsChanged();
		}

		// Stop mode (sprint only)
		if (showStopMode) {
			static const char* stopModeNames[] = { "Sprint State", "Input Release", "Speed-Based" };
			if (ImGui::Combo(fmt::format("Stop Mode##{}", label).c_str(), &settings->sprintNoiseStopMode, stopModeNames, 3)) {
				MarkSettingsChanged();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"How to detect when to start the noise blend-out.\n\n"
					"Sprint State: Wait for sprint state to end.\n"
					"Input Release: Start fading on sprint key release.\n"
					"Speed-Based: Fade proportionally to deceleration.");
			}
		}

		ImGui::Spacing();

		// Position amplitudes
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.35f, 0.2f, 0.6f));
		if (ImGui::TreeNode(fmt::format("Position Amplitude##{}", label).c_str())) {
			ImGui::PopStyleColor();
			if (SliderFloatWithTooltip(fmt::format("X - Lateral Sway##{}", label).c_str(), &params.posAmpX, 0.0f, 0.3f, "%.3f",
				"Side-to-side sway amplitude.")) {
				params.preset = 0; MarkSettingsChanged();
			}
			if (SliderFloatWithTooltip(fmt::format("Y - Forward/Back##{}", label).c_str(), &params.posAmpY, 0.0f, 0.2f, "%.3f",
				"Forward/backward bob amplitude.")) {
				params.preset = 0; MarkSettingsChanged();
			}
			if (SliderFloatWithTooltip(fmt::format("Z - Vertical Bob##{}", label).c_str(), &params.posAmpZ, 0.0f, 0.3f, "%.3f",
				"Up/down bob amplitude.")) {
				params.preset = 0; MarkSettingsChanged();
			}
			ImGui::TreePop();
		} else {
			ImGui::PopStyleColor();
		}

		// Rotation amplitudes
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.35f, 0.6f));
		if (ImGui::TreeNode(fmt::format("Rotation Amplitude (degrees)##{}", label).c_str())) {
			ImGui::PopStyleColor();
			if (SliderFloatWithTooltip(fmt::format("Pitch (Nod)##{}", label).c_str(), &params.rotAmpX, 0.0f, 3.0f, "%.2f",
				"Head nod up/down with each stride.")) {
				params.preset = 0; MarkSettingsChanged();
			}
			if (SliderFloatWithTooltip(fmt::format("Roll (Tilt)##{}", label).c_str(), &params.rotAmpY, 0.0f, 3.0f, "%.2f",
				"Head tilt left/right with each stride.")) {
				params.preset = 0; MarkSettingsChanged();
			}
			if (SliderFloatWithTooltip(fmt::format("Yaw (Look)##{}", label).c_str(), &params.rotAmpZ, 0.0f, 2.0f, "%.2f",
				"Subtle left/right look with each stride.")) {
				params.preset = 0; MarkSettingsChanged();
			}
			ImGui::TreePop();
		} else {
			ImGui::PopStyleColor();
		}

		// Rhythm shape
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.35f, 0.25f, 0.2f, 0.6f));
		if (ImGui::TreeNode(fmt::format("Rhythm Shape##{}", label).c_str())) {
			ImGui::PopStyleColor();
			if (SliderFloatWithTooltip(fmt::format("Vertical Bias##{}", label).c_str(), &params.verticalBias, 0.0f, 1.0f, "%.2f",
				"0 = symmetric sine, 1 = sharp-down/soft-up (impact feel).")) {
				params.preset = 0; MarkSettingsChanged();
			}
			if (SliderFloatWithTooltip(fmt::format("Second Harmonic##{}", label).c_str(), &params.secondHarmonic, 0.0f, 1.0f, "%.2f",
				"Double-frequency overtone amount (0-1).")) {
				params.preset = 0; MarkSettingsChanged();
			}
			if (SliderFloatWithTooltip(fmt::format("Lateral Phase##{}", label).c_str(), &params.lateralPhase, 0.0f, 1.0f, "%.2f",
				"0.5 = alternating left/right per stride (natural).")) {
				params.preset = 0; MarkSettingsChanged();
			}
			ImGui::TreePop();
		} else {
			ImGui::PopStyleColor();
		}

		// Copy Settings From
		ImGui::Spacing();
		const char* copyFromNames[] = { "Walking", "Running", "Sprinting" };
		int copyFromIdx = -1;
		ImGui::SetNextItemWidth(150.0f);
		if (ImGui::BeginCombo(fmt::format("Copy Settings From##{}", label).c_str(), "Select...")) {
			for (int i = 0; i < 3; ++i) {
				if (static_cast<int>(layer) == i) continue;
				if (ImGui::Selectable(copyFromNames[i], false)) {
					copyFromIdx = i;
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Copy all tunable parameters from another movement layer.");
		}
		if (copyFromIdx >= 0) {
			MovementNoiseParams* source = nullptr;
			switch (copyFromIdx) {
			case 0: source = &settings->walkNoise; break;
			case 1: source = &settings->runNoise; break;
			case 2: source = &settings->sprintNoise; break;
			}
			if (source) {
				params.CopyTunablesFrom(*source);
				params.preset = 0;
				MarkSettingsChanged();
			}
		}
	}

	void DrawMovementNoiseSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("Movement Camera Noise", State::movementNoiseExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::movementNoiseExpanded = true;
			
			ImGui::TextWrapped("Rhythmic camera movement (head bob/sway) for walking, running, and sprinting. Each layer crossfades smoothly during movement transitions.");
			ImGui::Spacing();
			
			ImGui::BeginDisabled(!State::editMode);

			// === WALKING ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.35f, 0.25f, 0.6f));
			if (ImGui::CollapsingHeader("Walking##MovNoise", State::walkNoiseExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
				State::walkNoiseExpanded = true;
				ImGui::PopStyleColor();
				ImGui::Indent(8.0f);
				DrawNoiseLayerUI("WalkNoise", settings->walkNoise, NoiseLayer::Walk, settings, false);
				ImGui::Unindent(8.0f);
			} else {
				State::walkNoiseExpanded = false;
				ImGui::PopStyleColor();
			}

			ImGui::Spacing();

			// === RUNNING ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.3f, 0.4f, 0.6f));
			if (ImGui::CollapsingHeader("Running##MovNoise", State::runNoiseExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
				State::runNoiseExpanded = true;
				ImGui::PopStyleColor();
				ImGui::Indent(8.0f);
				DrawNoiseLayerUI("RunNoise", settings->runNoise, NoiseLayer::Run, settings, false);
				ImGui::Unindent(8.0f);
			} else {
				State::runNoiseExpanded = false;
				ImGui::PopStyleColor();
			}

			ImGui::Spacing();

			// === SPRINTING ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.25f, 0.2f, 0.6f));
			if (ImGui::CollapsingHeader("Sprinting##MovNoise", State::sprintNoiseExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
				State::sprintNoiseExpanded = true;
				ImGui::PopStyleColor();
				ImGui::Indent(8.0f);
				DrawNoiseLayerUI("SprintNoise", settings->sprintNoise, NoiseLayer::Sprint, settings, true);
				ImGui::Unindent(8.0f);
			} else {
				State::sprintNoiseExpanded = false;
				ImGui::PopStyleColor();
			}

			ImGui::EndDisabled();
		} else {
			State::movementNoiseExpanded = false;
		}
	}

	void DrawFovPunchSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("FOV Punch", State::fovPunchExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::fovPunchExpanded = true;
			
			ImGui::TextWrapped("Quick FOV punch that dips in, overshoots, then returns to normal.");
			ImGui::Spacing();
			
			ImGui::BeginDisabled(!State::editMode);
			
			if (SliderFloatWithTooltip("Punch Duration", &settings->fovPunchDuration, 0.05f, 1.0f, "%.2f sec",
				"Total time for the full punch (in, overshoot, return).")) {
				MarkSettingsChanged();
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("On Taking Hit:");
			
			if (CheckboxWithTooltip("Enable Hit Punch", &settings->fovPunchHitEnabled,
				"Apply a quick FOV punch when the player takes a hit (excludes damage over time).")) {
				MarkSettingsChanged();
			}
			
			if (settings->fovPunchHitEnabled) {
				if (SliderFloatWithTooltip("Hit Strength", &settings->fovPunchHitStrength, 0.0f, 15.0f, "%.1f%%",
					"Percent of current FOV to punch in/out.\nExample: 5.0 = -5% then +5%.")) {
					MarkSettingsChanged();
				}
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("On Arrow/Bolt Release:");
			
			if (CheckboxWithTooltip("Enable Arrow Punch", &settings->fovPunchArrowEnabled,
				"Apply a quick FOV punch when firing a bow or crossbow.")) {
				MarkSettingsChanged();
			}
			
			if (settings->fovPunchArrowEnabled) {
				if (SliderFloatWithTooltip("Arrow Strength", &settings->fovPunchArrowStrength, 0.0f, 15.0f, "%.1f%%",
					"Percent of current FOV to punch in/out.\nExample: 3.0 = -3% then +3%.")) {
					MarkSettingsChanged();
				}
			}
			
			ImGui::EndDisabled();
		} else {
			State::fovPunchExpanded = false;
		}
	}
	
	void DrawFallEffectSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("Falling Disorientation (Mirror's Edge)", State::fallEffectExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::fallEffectExpanded = true;
			
			ImGui::TextWrapped("Mirror's-Edge style disorientation effect for falls: wind + tinnitus audio, procedural camera shake, double vision, FOV oscillation. Audio plays only if loose WAV files are present at Data/SKSE/Plugins/FPCameraSettle/.");
			ImGui::Spacing();
			
			ImGui::BeginDisabled(!State::editMode);
			
			if (CheckboxWithTooltip("Enable Fall Effect", &settings->fallEffectEnabled,
				"Master toggle for the entire falling disorientation effect.\n"
				"All sub-effects (audio/shake/visual/FOV) require this to be on.")) {
				MarkSettingsChanged();
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Trigger Conditions:");
			
			if (SliderFloatWithTooltip("Trigger Air Time", &settings->fallTriggerTime, 0.0f, 3.0f, "%.2f sec",
				"Seconds the player must be in midair before the effect can begin.")) {
				MarkSettingsChanged();
			}
			if (SliderFloatWithTooltip("Trigger Velocity", &settings->fallTriggerVelocity, 0.0f, 3000.0f, "%.0f units/s",
				"Downward velocity threshold (units/sec) to trigger the effect.")) {
				MarkSettingsChanged();
			}
			if (CheckboxWithTooltip("Require Both Conditions", &settings->fallRequireBothConditions,
				"If enabled, BOTH air time AND velocity thresholds must be exceeded.\n"
				"If disabled, EITHER one is enough.")) {
				MarkSettingsChanged();
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Phase Timing:");
			
			if (SliderFloatWithTooltip("Phase 1 Duration", &settings->fallPhase1Duration, 0.1f, 5.0f, "%.2f sec",
				"How long Phase 1 (subtle wind/shake) lasts before Phase 2 begins.")) {
				MarkSettingsChanged();
			}
			if (SliderFloatWithTooltip("Phase 2 Duration", &settings->fallPhase2Duration, 0.1f, 5.0f, "%.2f sec",
				"How long Phase 2 (whine + double-vision intro) lasts before Phase 3 (full intensity) begins.")) {
				MarkSettingsChanged();
			}
			
			ImGui::Spacing();
			
			// === CAMERA SHAKE ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.2f, 0.2f, 0.6f));
			if (ImGui::TreeNode("Camera Shake##FallShake")) {
				ImGui::PopStyleColor();
				
				if (CheckboxWithTooltip("Enable Shake", &settings->fallShakeEnabled, "Enable procedural camera shake during falling")) {
					MarkSettingsChanged();
				}
				
				if (SliderFloatWithTooltip("Master Intensity##FallShake", &settings->fallShakeIntensity, 0.0f, 3.0f, "%.2f",
					"Master multiplier for shake amplitude.")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Fade In##FallShake", &settings->fallShakeFadeIn, 0.0f, 10.0f, "%.2f sec",
					"Time from fall start to full shake intensity.\n\n"
					"Uses the Fade Curve setting from the Audio section.\n"
					"Longer values give a more gradual build-up.")) {
					settings->fallShakeFadeIn = std::clamp(settings->fallShakeFadeIn, 0.0f, 10.0f);
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Position Scale##FallShake", &settings->fallShakePosScale, 0.0f, 3.0f, "%.2f",
					"Scale factor for the position component of shake.")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Rotation Scale##FallShake", &settings->fallShakeRotScale, 0.0f, 3.0f, "%.2f",
					"Scale factor for the rotation component of shake.")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Frequency##FallShake", &settings->fallShakeFrequency, 0.5f, 30.0f, "%.1f Hz",
					"Base oscillation frequency for the sine component.")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Noise Amount##FallShake", &settings->fallShakeNoiseAmount, 0.0f, 1.0f, "%.2f",
					"Mix between sine (0) and value-noise (1).\n"
					"0 = pure sine wave shake (smooth, predictable)\n"
					"1 = pure noise shake (jittery, organic)\n"
					"0.5 = balanced mix")) {
					MarkSettingsChanged();
				}
				
				ImGui::Separator();
				ImGui::Text("Axes:");
				if (CheckboxWithTooltip("Position##FallShakeAxis", &settings->fallShakeAffectPosition, "Apply shake to camera position")) MarkSettingsChanged();
				ImGui::SameLine();
				if (CheckboxWithTooltip("Pitch##FallShakeAxis", &settings->fallShakeAffectPitch, "Allow pitch (look up/down) component")) MarkSettingsChanged();
				ImGui::SameLine();
				if (CheckboxWithTooltip("Yaw##FallShakeAxis", &settings->fallShakeAffectYaw, "Allow yaw (look left/right) component")) MarkSettingsChanged();
				ImGui::SameLine();
				if (CheckboxWithTooltip("Roll##FallShakeAxis", &settings->fallShakeAffectRoll, "Allow roll (head tilt) component")) MarkSettingsChanged();
				
				ImGui::Separator();
				if (SliderFloatWithTooltip("Downward Bias##FallShake", &settings->fallShakeDownwardBias, 0.0f, 5.0f, "%.2f deg",
					"Pitch bias (downward) added in Phase 3 to simulate the head being pulled down by wind.")) {
					MarkSettingsChanged();
				}
				if (CheckboxWithTooltip("Scale by Velocity##FallShake", &settings->fallShakeScaleByVelocity,
					"Increase shake intensity as fall velocity increases.")) {
					MarkSettingsChanged();
				}
				
				ImGui::TreePop();
			} else {
				ImGui::PopStyleColor();
			}
			
			// === AUDIO ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.3f, 0.4f, 0.6f));
			if (ImGui::TreeNode("Audio##FallAudio")) {
				ImGui::PopStyleColor();
				
				if (CheckboxWithTooltip("Enable Audio##FallAudio", &settings->fallAudioEnabled,
					"Master toggle for fall audio (wind + whine).\n\n"
					"Sound files must be placed at:\n"
					"Data/SKSE/Plugins/FPCameraSettle/fallingwindloop.wav\n"
					"Data/SKSE/Plugins/FPCameraSettle/fallingwhineloop.wav")) {
					MarkSettingsChanged();
				}
				
				{
					float pct = settings->fallMasterVolume * 100.0f;
					if (SliderFloatWithTooltip("Master Volume##FallAudio", &pct, 0.0f, 500.0f, "%.0f%%",
						"Master volume for ALL fall audio.\n\n"
						"0% = mute, 100% = native, up to 500%.\n"
						"Volume is applied per-sample in software — does not\n"
						"affect game music or other system sounds.")) {
						settings->fallMasterVolume = std::clamp(pct / 100.0f, 0.0f, 5.0f);
						MarkSettingsChanged();
					}
				}
				if (CheckboxWithTooltip("Volume by Velocity##FallAudio", &settings->fallAudioVolumeByVelocity,
					"Scale audio volume up as fall velocity increases.")) {
					MarkSettingsChanged();
				}
				
				ImGui::Separator();
				ImGui::Text("Fade Settings:");
				{
					const char* curveNames[] = { "Linear", "Smooth (S-Curve)", "Ease In (slow start)", "Ease Out (fast start)", "Exponential (very slow)" };
					if (ImGui::Combo("Fade Curve##FallAudio", &settings->fallFadeCurve, curveNames, 5)) {
						MarkSettingsChanged();
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip(
							"Shape of the volume fade-in curve.\n\n"
							"Linear: constant ramp, even throughout.\n"
							"Smooth: S-curve, accelerates in the middle.\n"
							"Ease In: slow start, builds gradually (quadratic).\n"
							"Ease Out: fast start, plateaus gently.\n"
							"Exponential: very slow start, late build (cubic).");
					}
				}
				if (SliderFloatWithTooltip("Wind Fade-In##FallAudio", &settings->fallWindFadeIn, 0.0f, 10.0f, "%.2f sec",
					"How long the wind loop takes to ramp from silence to full\n"
					"volume after the fall starts.\n\n"
					"Longer values + Ease In/Exponential curve give the most\n"
					"gradual, barely-perceptible build.")) {
					settings->fallWindFadeIn = std::clamp(settings->fallWindFadeIn, 0.0f, 10.0f);
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Whine Fade-In##FallAudio", &settings->fallWhineFadeIn, 0.0f, 10.0f, "%.2f sec",
					"How long the whine takes to ramp from silence to full\n"
					"volume from the start of Phase 2.")) {
					settings->fallWhineFadeIn = std::clamp(settings->fallWhineFadeIn, 0.0f, 10.0f);
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Fade Out Time##FallAudio", &settings->fallAudioFadeOut, 0.05f, 5.0f, "%.2f sec",
					"How long audio takes to fade out after landing.")) {
					settings->fallAudioFadeOut = std::clamp(settings->fallAudioFadeOut, 0.05f, 5.0f);
					MarkSettingsChanged();
				}
				
				// Reload button + presence status
				ImGui::Spacing();
				if (ImGui::Button("Reload Audio Files##FallAudio")) {
					FallEffect::FallEffectManager::GetSingleton()->ReloadAudio();
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Closes the audio handles and re-checks both WAV files on disk.\n"
						"Click after dropping new files into Data/SKSE/Plugins/FPCameraSettle/.");
				}
				
				auto* fallMgr = FallEffect::FallEffectManager::GetSingleton();
				
				ImGui::Separator();
				ImGui::Text("Wind Loop:");
				if (CheckboxWithTooltip("Enable Wind##FallAudio", &settings->fallWindEnabled, "Play looping wind audio during fall")) MarkSettingsChanged();
				{
					float pct = settings->fallWindMaxVolume * 100.0f;
					if (SliderFloatWithTooltip("Wind Max Volume##FallAudio", &pct, 0.0f, 500.0f, "%.0f%%",
						"Wind loop maximum volume (0-500%).\n"
						"Above 100% amplifies audio in software with clipping.")) {
						settings->fallWindMaxVolume = std::clamp(pct / 100.0f, 0.0f, 5.0f);
						MarkSettingsChanged();
					}
				}
				if (fallMgr->IsWindFilePresent()) {
					ImGui::TextColored(ImVec4(0.55f, 0.95f, 0.55f, 1.0f),
						"FOUND: Data/SKSE/Plugins/FPCameraSettle/fallingwindloop.wav");
				} else {
					ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
						"MISSING: Data/SKSE/Plugins/FPCameraSettle/fallingwindloop.wav");
				}
				
				ImGui::Separator();
				ImGui::Text("Whine Loop (tinnitus):");
				if (CheckboxWithTooltip("Enable Whine##FallAudio", &settings->fallWhineEnabled, "Play looping high-pitched whine audio (Phase 2+)")) MarkSettingsChanged();
				{
					float pct = settings->fallWhineMaxVolume * 100.0f;
					if (SliderFloatWithTooltip("Whine Max Volume##FallAudio", &pct, 0.0f, 500.0f, "%.0f%%",
						"Whine loop maximum volume (0-500%).\n"
						"Above 100% amplifies audio in software with clipping.")) {
						settings->fallWhineMaxVolume = std::clamp(pct / 100.0f, 0.0f, 5.0f);
						MarkSettingsChanged();
					}
				}
				if (fallMgr->IsWhineFilePresent()) {
					ImGui::TextColored(ImVec4(0.55f, 0.95f, 0.55f, 1.0f),
						"FOUND: Data/SKSE/Plugins/FPCameraSettle/fallingwhineloop.wav");
				} else {
					ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
						"MISSING: Data/SKSE/Plugins/FPCameraSettle/fallingwhineloop.wav");
				}
				
				ImGui::TreePop();
			} else {
				ImGui::PopStyleColor();
			}
			
			// === VISUAL ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.4f, 0.6f));
			if (ImGui::TreeNode("Visual Effects##FallVisual")) {
				ImGui::PopStyleColor();
				
				ImGui::Text("Double Vision (Phase 2+):");
				if (CheckboxWithTooltip("Enable Double Vision##FallVisual", &settings->fallDoubleVisionEnabled, "Apply ghosting/double-vision overlay during fall")) MarkSettingsChanged();
				if (SliderFloatWithTooltip("Double Vision Strength##FallVisual", &settings->fallDoubleVisionMaxStrength, 0.0f, 2.0f, "%.2f", "Maximum strength of the double-vision overlay")) MarkSettingsChanged();
				if (SliderFloatWithTooltip("DV Fade In##FallVisual", &settings->fallDoubleVisionFadeIn, 0.0f, 10.0f, "%.2f sec",
					"Time from Phase 2 start to full double-vision strength.\n\n"
					"Uses the Fade Curve setting from the Audio section.")) {
					settings->fallDoubleVisionFadeIn = std::clamp(settings->fallDoubleVisionFadeIn, 0.0f, 10.0f);
					MarkSettingsChanged();
				}
				
				ImGui::Separator();
				ImGui::Text("Motion / Radial Blur (Phase 3):");
				if (CheckboxWithTooltip("Enable Motion Blur##FallVisual", &settings->fallMotionBlurEnabled, "Apply radial blur during peak fall intensity")) MarkSettingsChanged();
				if (SliderFloatWithTooltip("Motion Blur Strength##FallVisual", &settings->fallMotionBlurMaxStrength, 0.0f, 2.0f, "%.2f", "Maximum strength of the radial/motion blur")) MarkSettingsChanged();
				if (SliderFloatWithTooltip("Blur Fade In##FallVisual", &settings->fallMotionBlurFadeIn, 0.0f, 10.0f, "%.2f sec",
					"Time from Phase 3 start to full motion blur strength.\n\n"
					"Uses the Fade Curve setting from the Audio section.")) {
					settings->fallMotionBlurFadeIn = std::clamp(settings->fallMotionBlurFadeIn, 0.0f, 10.0f);
					MarkSettingsChanged();
				}
				
				ImGui::TreePop();
			} else {
				ImGui::PopStyleColor();
			}
			
			// === FOV OSCILLATION ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.3f, 0.2f, 0.6f));
			if (ImGui::TreeNode("FOV Oscillation##FallFOV")) {
				ImGui::PopStyleColor();
				
				ImGui::TextWrapped("Subtle FOV oscillation in Phase 3 to enhance disorientation.");
				ImGui::Spacing();
				
				if (CheckboxWithTooltip("Enable FOV Oscillation##FallFOV", &settings->fallFovEnabled, "Oscillate FOV during peak fall (Phase 3)")) MarkSettingsChanged();
				if (SliderFloatWithTooltip("Amplitude##FallFOV", &settings->fallFovOscAmplitude, 0.0f, 15.0f, "%.1f deg", "FOV oscillation amplitude (degrees, peak-to-peak / 2)")) MarkSettingsChanged();
				if (SliderFloatWithTooltip("Frequency##FallFOV", &settings->fallFovOscFrequency, 0.1f, 5.0f, "%.2f Hz", "FOV oscillation frequency")) MarkSettingsChanged();
				
				ImGui::TreePop();
			} else {
				ImGui::PopStyleColor();
			}
			
			// === FATAL LANDING (greyed out - under development) ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.3f, 0.4f));
			ImGui::BeginDisabled(true);
			if (ImGui::TreeNode("Fatal Landing (Death Slam)##FallFatal")) {
				ImGui::PopStyleColor();
				ImGui::EndDisabled();
				
				ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "This feature is under development.");
				ImGui::Spacing();
				
				ImGui::BeginDisabled(true);
				
				if (CheckboxWithTooltip("Enable Fatal Landing##FallFatal", &settings->fallFatalEnabled,
					"Enable the fatal landing effect when the player dies from fall damage.\n"
					"Requires the main Fall Effect to also be enabled.")) {
					MarkSettingsChanged();
				}
				
				ImGui::Separator();
				ImGui::Text("Black Screen:");
				
				if (SliderFloatWithTooltip("Black Duration##FallFatal", &settings->fallFatalBlackDuration, 0.1f, 10.0f, "%.2f sec",
					"How long the screen stays fully black after impact.")) {
					settings->fallFatalBlackDuration = std::clamp(settings->fallFatalBlackDuration, 0.1f, 10.0f);
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Fade In Speed##FallFatal", &settings->fallFatalFadeInTime, 0.01f, 2.0f, "%.2f sec",
					"How fast the screen goes black on impact.\n"
					"Lower = more sudden (more like Mirror's Edge).")) {
					settings->fallFatalFadeInTime = std::clamp(settings->fallFatalFadeInTime, 0.01f, 2.0f);
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Fade Out Speed##FallFatal", &settings->fallFatalFadeOutTime, 0.1f, 5.0f, "%.2f sec",
					"How fast the screen fades back from black to gameplay.")) {
					settings->fallFatalFadeOutTime = std::clamp(settings->fallFatalFadeOutTime, 0.1f, 5.0f);
					MarkSettingsChanged();
				}
				
				ImGui::Separator();
				ImGui::Text("Audio:");
				
				{
					float pct = settings->fallFatalWhineBoost * 100.0f;
					if (SliderFloatWithTooltip("Whine Spike##FallFatal", &pct, 0.0f, 1000.0f, "%.0f%%",
						"Volume spike for the whine/tinnitus at the moment of impact.\n"
						"Multiplied with the configured whine max volume.\n"
						"200% = double the normal max volume.")) {
						settings->fallFatalWhineBoost = std::clamp(pct / 100.0f, 0.0f, 10.0f);
						MarkSettingsChanged();
					}
				}
				if (SliderFloatWithTooltip("Whine Decay##FallFatal", &settings->fallFatalWhineDecay, 0.1f, 5.0f, "%.2f sec",
					"How long the spiked whine takes to fade to silence after impact.")) {
					settings->fallFatalWhineDecay = std::clamp(settings->fallFatalWhineDecay, 0.1f, 5.0f);
					MarkSettingsChanged();
				}
				{
					float pct = settings->fallFatalImpactVolume * 100.0f;
					if (SliderFloatWithTooltip("Impact Volume##FallFatal", &pct, 0.0f, 500.0f, "%.0f%%",
						"Volume of the body-impact sound (fallingdeathimpact.wav).\n"
						"0% = mute, 100% = native, up to 500%.")) {
						settings->fallFatalImpactVolume = std::clamp(pct / 100.0f, 0.0f, 5.0f);
						MarkSettingsChanged();
					}
				}
				
				auto* fallMgr = FallEffect::FallEffectManager::GetSingleton();
				if (fallMgr->IsImpactFilePresent()) {
					ImGui::TextColored(ImVec4(0.55f, 0.95f, 0.55f, 1.0f),
						"FOUND: Data/SKSE/Plugins/FPCameraSettle/fallingdeathimpact.wav");
				} else {
					ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
						"MISSING: Data/SKSE/Plugins/FPCameraSettle/fallingdeathimpact.wav");
				}
				
				ImGui::EndDisabled();
				ImGui::TreePop();
			} else {
				ImGui::PopStyleColor();
				ImGui::EndDisabled();
			}
			
			ImGui::EndDisabled();
			
			// Live debug status read-out (read-only, useful for tuning)
			ImGui::Spacing();
			ImGui::Separator();
			auto* fallMgr = FallEffect::FallEffectManager::GetSingleton();
			const char* phaseName = "Inactive";
			switch (fallMgr->GetPhase()) {
				case FallEffect::Phase::Phase1:       phaseName = "Phase 1 (intro)";       break;
				case FallEffect::Phase::Phase2:       phaseName = "Phase 2 (whine)";       break;
				case FallEffect::Phase::Phase3:       phaseName = "Phase 3 (full)";        break;
				case FallEffect::Phase::Landing:      phaseName = "Landing (fade out)";    break;
				case FallEffect::Phase::FatalLanding: phaseName = "FATAL (death slam)";    break;
				default: break;
			}
			ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f),
				"Status: %s  fallTime=%.2fs  intensity=%.2f",
				phaseName, fallMgr->GetFallTime(), fallMgr->GetIntensity01());
		} else {
			State::fallEffectExpanded = false;
		}
	}
	
	void DrawLeanSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("Leaning", State::leanExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::leanExpanded = true;
			
			ImGui::TextWrapped("Lean around corners manually (keyboard) or automatically during ranged combat near walls.");
			ImGui::Spacing();
			
			ImGui::BeginDisabled(!State::editMode);
			
			if (CheckboxWithTooltip("Enable Leaning", &settings->leanEnabled,
				"Master toggle for the entire leaning system.")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Intensity##Lean", &settings->leanIntensity, 0.0f, 3.0f, "%.2f",
				"Master multiplier for lean camera and skeleton offsets.")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Blend Speed##Lean", &settings->leanBlendSpeed, 1.0f, 20.0f, "%.1f",
				"How fast the lean blends in when activated.")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Return Speed##Lean", &settings->leanReturnSpeed, 1.0f, 20.0f, "%.1f",
				"How fast the lean returns to center when released.")) {
				MarkSettingsChanged();
			}
			
			// Live status
			auto* leanMgr = Lean::LeanManager::GetSingleton();
			const char* sourceName = "None";
			switch (leanMgr->GetLeanSource()) {
				case Lean::LeanSource::Manual:     sourceName = "Manual"; break;
				case Lean::LeanSource::Contextual: sourceName = "Contextual"; break;
				default: break;
			}
			ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f),
				"Lean: %.2f  Source: %s", leanMgr->GetLeanCurrent(), sourceName);
			
			ImGui::Spacing();
			
			// === MANUAL LEAN ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.25f, 0.4f, 0.6f));
			if (ImGui::TreeNode("Manual Lean")) {
				ImGui::PopStyleColor();
				
				if (CheckboxWithTooltip("Enable Manual Lean", &settings->leanManualEnabled,
					"Allow leaning with keyboard keys.")) {
					MarkSettingsChanged();
				}
				
				static const char* modeNames[] = { "Hold", "Toggle" };
				if (ImGui::Combo("Mode##ManualLean", &settings->leanManualMode, modeNames, 2)) {
					MarkSettingsChanged();
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Hold: Lean while key is held down.\n"
						"Toggle: Press once to lean, press again to return.");
				}
				
				ImGui::Separator();
				ImGui::Text("Key Bindings:");

				// Poll Win32 for key capture while listening
				if (State::listeningForLeanLeft || State::listeningForLeanRight) {
					int pressed = PollForNewKeyPress();
					if (pressed >= 0) {
						if (pressed == 0x01) {
							State::listeningForLeanLeft = false;
							State::listeningForLeanRight = false;
						} else if (State::listeningForLeanLeft) {
							settings->leanLeftScancode = pressed;
							State::listeningForLeanLeft = false;
							MarkSettingsChanged();
						} else if (State::listeningForLeanRight) {
							settings->leanRightScancode = pressed;
							State::listeningForLeanRight = false;
							MarkSettingsChanged();
						}
					}
				}

				// --- Lean Left ---
				{
					const char* leftName = State::listeningForLeanLeft
						? ">> Press any key... <<"
						: GetKeyName(settings->leanLeftScancode);
					ImVec2 tsL; ImGui::CalcTextSize(&tsL, leftName, nullptr, false, -1.0f);
					float btnWidth = (tsL.x + 20.0f > 140.0f) ? tsL.x + 20.0f : 140.0f;
					ImGui::Text("Lean Left:");
					ImGui::SameLine();
					if (State::listeningForLeanLeft)
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.3f, 0.1f, 0.9f));
					if (ImGui::Button(fmt::format("{}##LeanLeftBtn", leftName).c_str(), ImVec2(btnWidth, 0))) {
						State::listeningForLeanLeft = !State::listeningForLeanLeft;
						State::listeningForLeanRight = false;
					}
					if (State::listeningForLeanLeft)
						ImGui::PopStyleColor();
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Click to bind a new key, then press any key/button.\nPress Escape to cancel.");
					}
					const char* conflictL = CheckKeyConflict(settings->leanLeftScancode);
					if (conflictL) {
						ImGui::SameLine();
						ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Conflicts with: %s", conflictL);
					}
				}

				// --- Lean Right ---
				{
					const char* rightName = State::listeningForLeanRight
						? ">> Press any key... <<"
						: GetKeyName(settings->leanRightScancode);
					ImVec2 tsR; ImGui::CalcTextSize(&tsR, rightName, nullptr, false, -1.0f);
					float btnWidth = (tsR.x + 20.0f > 140.0f) ? tsR.x + 20.0f : 140.0f;
					ImGui::Text("Lean Right:");
					ImGui::SameLine();
					if (State::listeningForLeanRight)
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.3f, 0.1f, 0.9f));
					if (ImGui::Button(fmt::format("{}##LeanRightBtn", rightName).c_str(), ImVec2(btnWidth, 0))) {
						State::listeningForLeanRight = !State::listeningForLeanRight;
						State::listeningForLeanLeft = false;
					}
					if (State::listeningForLeanRight)
						ImGui::PopStyleColor();
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Click to bind a new key, then press any key/button.\nPress Escape to cancel.");
					}
					const char* conflictR = CheckKeyConflict(settings->leanRightScancode);
					if (conflictR) {
						ImGui::SameLine();
						ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Conflicts with: %s", conflictR);
					}
				}
				
				ImGui::TreePop();
			} else {
				ImGui::PopStyleColor();
			}
			
			ImGui::Spacing();
			
			// === CONTEXTUAL LEAN ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.3f, 0.6f));
			if (ImGui::TreeNode("Contextual Lean (Auto)")) {
				ImGui::PopStyleColor();
				
				ImGui::TextWrapped("Automatic lean near walls during ranged combat. Raycasts left and right to detect cover.");
				ImGui::Spacing();
				
				if (CheckboxWithTooltip("Enable Contextual Lean", &settings->leanContextualEnabled,
					"Automatically lean toward open side when near a wall\n"
					"while in ranged combat stance (bow/crossbow/magic).")) {
					MarkSettingsChanged();
				}

				if (settings->leanContextualEnabled) {
					if (CheckboxWithTooltip("Gamepad Only##CtxLean", &settings->leanContextualGamepadOnly,
						"Only activate contextual lean when using a gamepad.\n"
						"Disables contextual lean when playing with keyboard/mouse.")) {
						MarkSettingsChanged();
					}
				}
				
				if (SliderFloatWithTooltip("Detection Distance", &settings->leanContextualDistance, 10.0f, 500.0f, "%.0f units",
					"Maximum raycast distance for wall detection.")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Shoulder Offset", &settings->leanContextualOffset, 5.0f, 100.0f, "%.1f units",
					"Lateral offset from camera for ray origins\n(simulates shoulder width).")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Deadzone", &settings->leanContextualDeadzone, 0.0f, 0.5f, "%.2f",
					"Minimum proximity ratio to trigger lean.\nHigher = need to be closer to wall.")) {
					MarkSettingsChanged();
				}
				
				ImGui::Separator();
				ImGui::Text("Weapon Types:");
				
				if (CheckboxWithTooltip("Bow##CtxLean", &settings->leanContextualBow,
					"Enable contextual lean while drawing a bow.")) {
					MarkSettingsChanged();
				}
				ImGui::SameLine();
				if (CheckboxWithTooltip("Crossbow##CtxLean", &settings->leanContextualCrossbow,
					"Enable contextual lean while aiming a crossbow.")) {
					MarkSettingsChanged();
				}
				ImGui::SameLine();
				if (CheckboxWithTooltip("Magic##CtxLean", &settings->leanContextualMagic,
					"Enable contextual lean while casting spells.")) {
					MarkSettingsChanged();
				}

				ImGui::Spacing();
				if (SliderFloatWithTooltip("Hold Time After Fire##CtxLean", &settings->leanContextualHoldTime,
					0.0f, 3.0f, "%.2f s",
					"How long the lean persists after firing/casting ends.\n"
					"Applies to bow, crossbow, and magic alike.")) {
					MarkSettingsChanged();
				}
				
				if (settings->leanContextualMagic) {
					if (CheckboxWithTooltip("Magic: Use Hand Origin##CtxLean", &settings->leanMagicUseHandOrigin,
						"When enabled, spells spawn from the actual hand node position\n"
						"(left or right depending on equipped slot) and are aimed toward\n"
						"the crosshair. When disabled, spells use a simple lateral offset\n"
						"like arrows/bolts.")) {
						MarkSettingsChanged();
					}
				}
				
				ImGui::TreePop();
			} else {
				ImGui::PopStyleColor();
			}
			
			ImGui::Spacing();
			
			// === CAMERA ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.35f, 0.3f, 0.2f, 0.6f));
			if (ImGui::TreeNode("Camera##Lean")) {
				ImGui::PopStyleColor();
				
				if (SliderFloatWithTooltip("Lateral Shift", &settings->leanPosAmount, 0.0f, 30.0f, "%.1f units",
					"How far the camera shifts sideways when fully leaned.")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Roll (Head Tilt)", &settings->leanRollDegrees, 0.0f, 30.0f, "%.1f deg",
					"Head tilt angle when fully leaned.")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Yaw (Look Around)", &settings->leanYawDegrees, 0.0f, 15.0f, "%.1f deg",
					"How much the camera rotates to look around the corner.")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("Forward Peek", &settings->leanForwardAmount, 0.0f, 20.0f, "%.1f units",
					"How far the camera pushes forward when leaning.")) {
					MarkSettingsChanged();
				}
				
				ImGui::TreePop();
			} else {
				ImGui::PopStyleColor();
			}
			
			ImGui::Spacing();
			
			// === SKELETON ===
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.4f, 0.6f));
			if (ImGui::TreeNode("Skeleton##Lean")) {
				ImGui::PopStyleColor();
				
				ImGui::Text("First Person:");
				if (CheckboxWithTooltip("Enable 1P Skeleton##Lean", &settings->leanFirstPersonEnabled,
					"Make first-person arms/weapon follow the lean.")) {
					MarkSettingsChanged();
				}
				if (SliderFloatWithTooltip("1P Scale##Lean", &settings->leanFirstPersonScale, 0.0f, 3.0f, "%.2f",
					"Scale multiplier for first-person skeleton lean.")) {
					MarkSettingsChanged();
				}
				static const char* spineNodeNames[] = { "NPC Spine [Spn0]", "NPC Spine1 [Spn1]", "NPC Spine2 [Spn2]" };
				if (ImGui::Combo("1P Lean Bone##Lean", &settings->leanFirstPersonNode, spineNodeNames, 3)) {
					MarkSettingsChanged();
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Which spine bone to apply the 1P lean rotation to.\n"
						"Spine2: Highest (arms move most, torso stays).\n"
						"Spine1: Middle.\n"
						"Spine:  Lowest (whole torso leans).");
				}
				
				ImGui::Separator();
				ImGui::BeginDisabled(true);
				ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Third-person body lean is under development.");
				ImGui::Text("Third Person:");
				bool tp_dummy = false;
				ImGui::Checkbox("Enable 3P Body##Lean", &tp_dummy);
				float tp_scale_dummy = 1.0f;
				ImGui::SliderFloat("3P Scale##Lean", &tp_scale_dummy, 0.0f, 3.0f, "%.2f");
				ImGui::EndDisabled();
				
				ImGui::TreePop();
			} else {
				ImGui::PopStyleColor();
			}
			
			ImGui::EndDisabled();
		} else {
			State::leanExpanded = false;
		}
	}

	void DrawDebugSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("Debug", State::debugExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::debugExpanded = true;
			
			if (CheckboxWithTooltip("Debug Logging", &settings->debugLogging,
				"Enable detailed debug messages in the log file")) {
				MarkSettingsChanged();
			}
			
			if (CheckboxWithTooltip("Debug On Screen", &settings->debugOnScreen,
				"Show debug information on screen")) {
				MarkSettingsChanged();
			}
			
			ImGui::Spacing();
			
			if (CheckboxWithTooltip("Enable Hot Reload", &settings->enableHotReload,
				"Automatically reload INI when file changes")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Hot Reload Interval", &settings->hotReloadIntervalSec, 1.0f, 60.0f, "%.0f sec",
				"How often to check for INI changes")) {
				MarkSettingsChanged();
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Quick Actions:");
			
			if (ImGui::Button("Reset Springs")) {
				CameraSettle::CameraSettleManager::GetSingleton()->Reset();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Reset all spring states to zero");
			}
		} else {
			State::debugExpanded = false;
		}
	}
	
	void DrawActionSettings()
	{
		auto* settings = Settings::GetSingleton();
		
		if (ImGui::CollapsingHeader("Per-Action Settings", State::actionSettingsExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			State::actionSettingsExpanded = true;
			
			ImGui::TextWrapped("Configure camera settle effects for each action type. Each action can have different settings for weapon drawn vs sheathed.");
			ImGui::Spacing();
			
			// Weapon state selector
			ImGui::Text("Editing:");
			ImGui::SameLine();
			if (ImGui::RadioButton("Weapon Drawn", State::showingDrawnSettings)) {
				State::showingDrawnSettings = true;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Weapon Sheathed", !State::showingDrawnSettings)) {
				State::showingDrawnSettings = false;
			}
			
			ImGui::Spacing();
			
			// Action type selector
			const char* actionNames[] = {
				"Walk Forward", "Walk Backward", "Walk Left", "Walk Right",
				"Run Forward", "Run Backward", "Run Left", "Run Right",
				"Sprint Forward",
				"Sneak Walk Forward", "Sneak Walk Backward", "Sneak Walk Left", "Sneak Walk Right",
				"Sneak Run Forward", "Sneak Run Backward", "Sneak Run Left", "Sneak Run Right",
				"Jump", "Land",
				"Sneak", "Un-Sneak",
				"Taking Hit", "Hitting",
				"Arrow Release"
			};
			
			ImGui::SetNextItemWidth(200.0f);
			if (ImGui::BeginCombo("Action Type", actionNames[State::selectedActionIndex])) {
				for (int i = 0; i < static_cast<int>(ActionType::kTotal); ++i) {
					bool isSelected = (State::selectedActionIndex == i);
					if (ImGui::Selectable(actionNames[i], isSelected)) {
						State::selectedActionIndex = i;
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			
			ImGui::Separator();
			
			// Get settings for selected action
			ActionType selectedAction = static_cast<ActionType>(State::selectedActionIndex);
			ActionSettings& actionSettings = settings->GetActionSettingsForState(selectedAction, State::showingDrawnSettings);
			
			DrawActionEditor(actionSettings, actionNames[State::selectedActionIndex], State::showingDrawnSettings);
			
			// Handle copy confirmation popup
			if (State::showCopyConfirmPopup) {
				ImGui::OpenPopup("Copy Settings?");
				State::showCopyConfirmPopup = false;
			}
			
			// Center the popup
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImVec2 center(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f);
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			
			if (ImGui::BeginPopupModal("Copy Settings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				const char* sourceName = State::showingDrawnSettings ? "Weapon Drawn" : "Weapon Sheathed";
				const char* destName = State::copyToDrawn ? "Weapon Drawn" : "Weapon Sheathed";
				
				ImGui::Text("Copy action settings:");
				ImGui::Spacing();
				ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 1.0f), "Action: %s", actionNames[State::selectedActionIndex]);
				ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "From: %s", sourceName);
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "To: %s", destName);
				ImGui::Spacing();
				ImGui::TextWrapped("This will overwrite the %s settings for this action.", destName);
				
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
				
				if (ImGui::Button("Copy", ImVec2(100, 0))) {
					// Perform the copy
					ActionSettings& sourceSettings = settings->GetActionSettingsForState(selectedAction, State::showingDrawnSettings);
					ActionSettings& destSettings = settings->GetActionSettingsForState(selectedAction, State::copyToDrawn);
					destSettings.CopyFrom(sourceSettings);
					MarkSettingsChanged();
					RE::DebugNotification(fmt::format("Copied {} to {}", sourceName, destName).c_str());
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(100, 0))) {
					ImGui::CloseCurrentPopup();
				}
				
				ImGui::EndPopup();
			}
			
			// Handle copy to action popup
			if (State::showCopyToActionPopup) {
				ImGui::OpenPopup("Copy to Action?");
				State::showCopyToActionPopup = false;
			}
			
			// Center the popup
			ImGuiViewport* viewport2 = ImGui::GetMainViewport();
			ImVec2 center2(viewport2->Pos.x + viewport2->Size.x * 0.5f, viewport2->Pos.y + viewport2->Size.y * 0.5f);
			ImGui::SetNextWindowPos(center2, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			
			if (ImGui::BeginPopupModal("Copy to Action?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				const char* sourceName = State::showingDrawnSettings ? "Weapon Drawn" : "Weapon Sheathed";
				
				ImGui::Text("Copy settings from:");
				ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "  %s (%s)", actionNames[State::selectedActionIndex], sourceName);
				
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
				
				ImGui::Text("Copy to:");
				
				// Target action selector
				ImGui::SetNextItemWidth(200.0f);
				if (ImGui::BeginCombo("Target Action", actionNames[State::copyTargetActionIndex])) {
					for (int i = 0; i < static_cast<int>(ActionType::kTotal); ++i) {
						bool isSelected = (State::copyTargetActionIndex == i);
						if (ImGui::Selectable(actionNames[i], isSelected)) {
							State::copyTargetActionIndex = i;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				
				// Target weapon state selector
				ImGui::Text("Target State:");
				ImGui::SameLine();
				if (ImGui::RadioButton("Drawn##target", State::copyTargetIsDrawn)) {
					State::copyTargetIsDrawn = true;
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("Sheathed##target", !State::copyTargetIsDrawn)) {
					State::copyTargetIsDrawn = false;
				}
				
				ImGui::Spacing();
				
				// Show warning if copying to same action
				bool isSameAction = (State::copyTargetActionIndex == State::selectedActionIndex && 
				                     State::copyTargetIsDrawn == State::showingDrawnSettings);
				if (isSameAction) {
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Warning: Source and target are the same!");
				} else {
					const char* targetStateName = State::copyTargetIsDrawn ? "Weapon Drawn" : "Weapon Sheathed";
					ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 1.0f), "Will copy to: %s (%s)", 
						actionNames[State::copyTargetActionIndex], targetStateName);
				}
				
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
				
				// Disable copy button if same action
				if (isSameAction) {
					ImGui::BeginDisabled();
				}
				
				if (ImGui::Button("Copy", ImVec2(100, 0))) {
					// Perform the copy
					ActionSettings& sourceSettings = settings->GetActionSettingsForState(selectedAction, State::showingDrawnSettings);
					ActionType targetAction = static_cast<ActionType>(State::copyTargetActionIndex);
					ActionSettings& destSettings = settings->GetActionSettingsForState(targetAction, State::copyTargetIsDrawn);
					destSettings.CopyFrom(sourceSettings);
					MarkSettingsChanged();
					
					const char* targetStateName = State::copyTargetIsDrawn ? "Drawn" : "Sheathed";
					RE::DebugNotification(fmt::format("Copied {} to {} ({})", 
						actionNames[State::selectedActionIndex], 
						actionNames[State::copyTargetActionIndex],
						targetStateName).c_str());
					ImGui::CloseCurrentPopup();
				}
				
				if (isSameAction) {
					ImGui::EndDisabled();
				}
				
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(100, 0))) {
					ImGui::CloseCurrentPopup();
				}
				
				ImGui::EndPopup();
			}
		} else {
			State::actionSettingsExpanded = false;
		}
	}
	
	void DrawActionEditor(ActionSettings& settings, const char* label, bool isDrawn)
	{
		ImGui::PushID(label);
		ImGui::PushID(isDrawn ? "drawn" : "sheathed");
		
		// Master enable
		ImGui::PushStyleColor(ImGuiCol_Text, settings.enabled ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
		if (ImGui::Checkbox("Enable", &settings.enabled)) {
			MarkSettingsChanged();
		}
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Enable/disable this action's camera settle effect");
		}
		
		// Per-action intensity multiplier (0-10x)
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150.0f);
		if (SliderFloatWithTooltip("Multiplier", &settings.multiplier, 0.0f, 10.0f, "%.1fx",
			"Per-action intensity multiplier (0 = disabled, 10 = maximum)")) {
			settings.multiplier = std::clamp(settings.multiplier, 0.0f, 10.0f);
			MarkSettingsChanged();
		}
		
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.0f);
		if (SliderFloatWithTooltip("Blend", &settings.blendTime, 0.0f, 1.0f, "%.2fs",
			"Time to blend impulse into spring (0 = instant, up to 1.0 sec)")) {
			settings.blendTime = std::clamp(settings.blendTime, 0.0f, 1.0f);
			MarkSettingsChanged();
		}
		
		if (!settings.enabled) {
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
		}
		
		ImGui::Spacing();
		
		// Spring parameters
		if (ImGui::TreeNodeEx("Spring Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (SliderFloatWithTooltip("Stiffness", &settings.stiffness, 10.0f, 500.0f, "%.0f",
				"Spring stiffness (higher = faster return to center)")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Damping", &settings.damping, 1.0f, 50.0f, "%.1f",
				"Damping (higher = less oscillation)")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Position Strength", &settings.positionStrength, 0.0f, 30.0f, "%.1f",
				"Maximum position offset strength")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Rotation Strength", &settings.rotationStrength, 0.0f, 20.0f, "%.1f deg",
				"Maximum rotation offset strength (degrees)")) {
				MarkSettingsChanged();
			}
			
			ImGui::TreePop();
		}
		
		// Position impulse
		if (ImGui::TreeNodeEx("Position Impulse", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Direction of initial camera movement");
			
			if (SliderFloatWithTooltip("X (Left/Right)", &settings.impulseX, -20.0f, 20.0f, "%.1f",
				"Horizontal impulse (-left, +right)")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Y (Forward/Back)", &settings.impulseY, -20.0f, 20.0f, "%.1f",
				"Depth impulse (+forward, -back)")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Z (Up/Down)", &settings.impulseZ, -20.0f, 20.0f, "%.1f",
				"Vertical impulse (+up, -down)")) {
				MarkSettingsChanged();
			}
			
			ImGui::TreePop();
		}
		
		// Rotation impulse
		if (ImGui::TreeNodeEx("Rotation Impulse", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Direction of initial camera rotation");
			
			if (SliderFloatWithTooltip("Pitch (X)", &settings.rotImpulseX, -15.0f, 15.0f, "%.1f deg",
				"Pitch impulse (+look up, -look down)")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Roll (Y)", &settings.rotImpulseY, -15.0f, 15.0f, "%.1f deg",
				"Roll impulse (+tilt right, -tilt left)")) {
				MarkSettingsChanged();
			}
			
			if (SliderFloatWithTooltip("Yaw (Z)", &settings.rotImpulseZ, -15.0f, 15.0f, "%.1f deg",
				"Yaw impulse (+look left, -look right)")) {
				MarkSettingsChanged();
			}
			
			ImGui::TreePop();
		}
		
		// Reset button and Copy button
		ImGui::Spacing();
		if (ImGui::Button("Reset to Defaults")) {
			settings.enabled = true;
			settings.multiplier = 1.0f;
			settings.blendTime = 0.1f;
			settings.stiffness = 100.0f;
			settings.damping = 8.0f;
			settings.positionStrength = 5.0f;
			settings.rotationStrength = 3.0f;
			settings.impulseX = 0.0f;
			settings.impulseY = 0.0f;
			settings.impulseZ = 0.0f;
			settings.rotImpulseX = 0.0f;
			settings.rotImpulseY = 0.0f;
			settings.rotImpulseZ = 0.0f;
			MarkSettingsChanged();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Reset this action to default values");
		}
		
		// Copy button - copies current action settings to the other weapon state
		ImGui::SameLine();
		const char* copyButtonLabel = isDrawn ? "Copy to Sheathed" : "Copy to Drawn";
		if (ImGui::Button(copyButtonLabel)) {
			State::showCopyConfirmPopup = true;
			State::copyToDrawn = !isDrawn;  // Copy to the opposite state
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(isDrawn 
				? "Copy these settings to the Weapon Sheathed version of this action" 
				: "Copy these settings to the Weapon Drawn version of this action");
		}
		
		// Copy to another action button
		ImGui::SameLine();
		if (ImGui::Button("Copy to Action...")) {
			State::showCopyToActionPopup = true;
			State::copyTargetActionIndex = State::selectedActionIndex;  // Default to current
			State::copyTargetIsDrawn = State::showingDrawnSettings;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Copy these settings to a different action type");
		}
		
		if (!settings.enabled) {
			ImGui::PopStyleVar();
		}
		
		ImGui::PopID();
		ImGui::PopID();
	}
	
	void DrawSaveLoadButtons()
	{
		auto* settings = Settings::GetSingleton();
		
		// Save button
		if (ImGui::Button("Save to INI")) {
			settings->Save();
			State::hasUnsavedChanges = false;
			RE::DebugNotification("FP Camera Settle: Settings saved");
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Save all settings to FPCameraSettle.ini");
		}
		
		ImGui::SameLine();
		
		// Reload button
		if (ImGui::Button("Reload from INI")) {
			settings->Load();
			State::hasUnsavedChanges = false;
			RE::DebugNotification("FP Camera Settle: Settings reloaded");
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Reload all settings from INI file");
		}
		
		ImGui::SameLine();
		
		// Reset All button
		if (ImGui::Button("Reset All to Defaults")) {
			settings->Load();  // This reloads from INI which has defaults
			MarkSettingsChanged();
			RE::DebugNotification("FP Camera Settle: Settings reset");
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Reset all settings to plugin defaults");
		}
		
		// Status text
		if (State::hasUnsavedChanges) {
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), 
				"You have unsaved changes. Save to keep them after restart.");
		} else {
			ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), 
				"All settings saved.");
		}
		
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Path: Data/SKSE/Plugins/FPCameraSettle.ini");
	}
}

