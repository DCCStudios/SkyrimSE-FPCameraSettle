#pragma once

#include "SKSEMenuFramework.h"
#include "Settings.h"

namespace Menu
{
	// Register with SKSE Menu Framework
	void Register();
	
	// Main render function
	void __stdcall Render();
	
	// Helper functions
	bool SliderFloatWithTooltip(const char* label, float* value, float min, float max, const char* format, const char* tooltip);
	bool CheckboxWithTooltip(const char* label, bool* value, const char* tooltip);
	
	// Section renderers
	void DrawHeader();
	void DrawGeneralSettings();
	void DrawWeaponStateSettings();
	void DrawMovementSettings();
	void DrawJumpSettings();
	void DrawSettlingSettings();
	void DrawIdleNoiseSettings();
	void DrawSprintEffectsSettings();
	void DrawMovementNoiseSettings();
	void DrawFovPunchSettings();
	void DrawFallEffectSettings();
	void DrawLeanSettings();
	void DrawDebugSettings();
	void DrawActionSettings();
	void DrawActionEditor(ActionSettings& settings, const char* label, bool isDrawn);
	void DrawSaveLoadButtons();
	
	// Convert an encoded scancode (keyboard 0-255, mouse 256-265, gamepad 266+) to a display name
	const char* GetKeyName(int a_encoded);

	// Check if an encoded scancode conflicts with a common game control.
	// Returns the conflicting action name, or nullptr if no conflict.
	const char* CheckKeyConflict(int a_encoded);

	// UI State
	struct State
	{
		static inline bool initialized{ false };
		static inline bool hasUnsavedChanges{ false };
		static inline bool editMode{ false };  // When true, settings changes apply immediately
		
		// Expansion states
		static inline bool generalExpanded{ true };
		static inline bool weaponStateExpanded{ true };
		static inline bool movementExpanded{ false };
		static inline bool jumpExpanded{ false };
		static inline bool settlingExpanded{ false };
		static inline bool idleNoiseExpanded{ false };
		static inline bool sprintEffectsExpanded{ false };
		static inline bool movementNoiseExpanded{ false };
		static inline bool walkNoiseExpanded{ false };
		static inline bool runNoiseExpanded{ false };
		static inline bool sprintNoiseExpanded{ false };
		static inline bool fovPunchExpanded{ false };
		static inline bool fallEffectExpanded{ false };
		static inline bool leanExpanded{ false };
		static inline bool debugExpanded{ false };
		static inline bool actionSettingsExpanded{ true };
		
		// Selected action type
		static inline int selectedActionIndex{ 0 };
		static inline bool showingDrawnSettings{ true };
		
		// Copy confirmation popup state
		static inline bool showCopyConfirmPopup{ false };
		static inline bool copyToDrawn{ true };  // true = copy to drawn, false = copy to sheathed
		
		// Copy to action popup state
		static inline bool showCopyToActionPopup{ false };
		static inline int copyTargetActionIndex{ 0 };
		static inline bool copyTargetIsDrawn{ true };

		// Key binding listener state
		static inline bool listeningForLeanLeft{ false };
		static inline bool listeningForLeanRight{ false };
	};
	
	// Mark settings as changed (invalidates caches and marks unsaved)
	inline void MarkSettingsChanged()
	{
		State::hasUnsavedChanges = true;
		// Only mark dirty for live updates if edit mode is enabled
		if (State::editMode) {
			Settings::GetSingleton()->MarkDirty();
		}
	}
}

