#include "Menu.h"
#include "CameraSettle.h"

namespace Menu
{
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
		DrawSettlingSettings();
		DrawIdleNoiseSettings();
		DrawSprintEffectsSettings();
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
				if (SliderFloatWithTooltip("Blur Strength", &settings->sprintBlurStrength, 0.0f, 1.0f, "%.2f",
					"Intensity of the radial blur effect\n(0 = none, 1 = maximum)")) {
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
			}
			
			ImGui::EndDisabled();
		} else {
			State::sprintEffectsExpanded = false;
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

