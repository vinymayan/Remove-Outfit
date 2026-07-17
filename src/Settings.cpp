#include "Settings.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace
{
    constexpr const char* LANG_PATH = "Data/Viny Mods/Remove Outfit/Language.json";

    std::map<std::string, std::string> g_language;

    std::string ToLower(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    void FlattenLanguageNode(const nlohmann::json& node, const std::string& prefix)
    {
        if (node.is_string()) {
            g_language[prefix] = node.get<std::string>();
            return;
        }

        if (!node.is_object()) {
            return;
        }

        for (const auto& [key, value] : node.items()) {
            const auto childKey = prefix.empty() ? key : prefix + "." + key;
            FlattenLanguageNode(value, childKey);
        }
    }

    const char* GetTargetModeLabel(NPCTargetMode mode)
    {
        switch (mode) {
        case NPCTargetMode::kOnlyRecruitable:
            return RDO_UI::GetLoc("menu.target_only_recruitable", "Only Recruitable NPCs");
        case NPCTargetMode::kOnlySelected:
            return RDO_UI::GetLoc("menu.target_only_selected", "Only Selected NPCs");
        case NPCTargetMode::kAll:
        default:
            return RDO_UI::GetLoc("menu.target_all", "All NPCs");
        }
    }

    std::string FormatFormID(RE::FormID formID)
    {
        return std::format("{:08X}", formID);
    }

    std::string BuildNPCListLabel(const InternalFormInfo& info)
    {
        std::string label = info.GetDisplayName();
        if (!info.editorID.empty() && info.editorID != label) {
            label += " / " + info.editorID;
        }
        if (!info.pluginName.empty()) {
            label += " [" + info.pluginName + "]";
        }
        label += " (" + FormatFormID(info.formID) + ")";
        return label;
    }

    std::string BuildSelectedNPCLabel(const SelectedNPCInfo& info)
    {
        std::string label = info.displayName.empty() ? FormatFormID(info.formID) : info.displayName;
        if (!info.pluginName.empty()) {
            label += " [" + info.pluginName + "]";
        }
        if (info.formID != 0) {
            label += " (" + FormatFormID(info.formID) + ")";
        }
        return label;
    }

    bool DrawNPCDropdown(RE::FormID& selectedFormID)
    {
        bool changed = false;
        auto manager = Manager::GetSingleton();
        const auto& npcs = manager->GetList("NPC");
        if (npcs.empty()) {
            ImGuiMCP::TextDisabled("%s", RDO_UI::GetLoc("menu.no_npcs_available", "No NPCs are available yet."));
            return false;
        }

        std::vector<std::string> labels;
        std::vector<std::size_t> mapToNPC;
        labels.reserve(npcs.size() + 1);
        mapToNPC.reserve(npcs.size() + 1);

        labels.emplace_back(RDO_UI::GetLoc("common.none", "None"));
        mapToNPC.push_back(static_cast<std::size_t>(-1));

        int currentSelection = 0;
        for (std::size_t i = 0; i < npcs.size(); ++i) {
            labels.push_back(BuildNPCListLabel(npcs[i]));
            mapToNPC.push_back(i);
            if (npcs[i].formID == selectedFormID) {
                currentSelection = static_cast<int>(i) + 1;
            }
        }

        constexpr float npcComboWidth = 640.0f;
        constexpr float npcPopupWidth = 700.0f;

        ImGuiMCP::PushID("selected_npc_dropdown");
        ImGuiMCP::SetNextItemWidth(npcComboWidth);
        ImGuiMCP::SetNextWindowSizeConstraints(
            ImGuiMCP::ImVec2(npcPopupWidth, 0.0f),
            ImGuiMCP::ImVec2(npcPopupWidth, 1000.0f));
        if (ImGuiMCP::BeginCombo("##npc_dropdown", labels[currentSelection].c_str())) {
            static std::string searchBuffer;
            char searchBuf[256]{};
            strcpy_s(searchBuf, searchBuffer.c_str());

            ImGuiMCP::SetNextItemWidth(-1.0f);
            if (ImGuiMCP::InputText("##npc_filter", searchBuf, sizeof(searchBuf))) {
                searchBuffer = searchBuf;
            }

            ImGuiMCP::Separator();

            const auto search = ToLower(searchBuffer);
            ImGuiMCP::BeginChild("##npc_dropdown_scroll", ImGuiMCP::ImVec2(0, 320), false);
            for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
                if (!search.empty() && ToLower(labels[i]).find(search) == std::string::npos) {
                    continue;
                }

                const bool selected = currentSelection == i;
                if (ImGuiMCP::Selectable(labels[i].c_str(), selected)) {
                    const auto originalIndex = mapToNPC[i];
                    selectedFormID = originalIndex == static_cast<std::size_t>(-1) ? 0 : npcs[originalIndex].formID;
                    searchBuffer.clear();
                    changed = true;
                }
                if (selected) {
                    ImGuiMCP::SetItemDefaultFocus();
                }
            }
            ImGuiMCP::EndChild();
            ImGuiMCP::EndCombo();
        }
        ImGuiMCP::PopID();

        return changed;
    }

    bool RenderSelectedNPCTable()
    {
        bool changed = false;
        auto settings = NPCSettings::GetSingleton();

        static RE::FormID pendingNPC = 0;

        ImGuiMCP::Text("%s", RDO_UI::GetLoc("menu.add_selected_npc", "Add NPC"));
        DrawNPCDropdown(pendingNPC);
        ImGuiMCP::SameLine();

        const bool canAdd = pendingNPC != 0;
        if (!canAdd) {
            ImGuiMCP::BeginDisabled();
        }

        if (ImGuiMCP::Button(RDO_UI::GetLoc("menu.add", "Add"))) {
            if (auto npc = RE::TESForm::LookupByID<RE::TESNPC>(pendingNPC); settings->AddSelectedNPC(npc)) {
                pendingNPC = 0;
                changed = true;
            }
        }

        if (!canAdd) {
            ImGuiMCP::EndDisabled();
        }

        ImGuiMCP::Spacing();

        if (ImGuiMCP::BeginTable("##selected_npcs_table", 3, ImGuiMCP::ImGuiTableFlags_Borders | ImGuiMCP::ImGuiTableFlags_RowBg)) {
            ImGuiMCP::TableSetupColumn(RDO_UI::GetLoc("menu.npc_name", "NPC"));
            ImGuiMCP::TableSetupColumn(RDO_UI::GetLoc("menu.npc_source", "Source"));
            ImGuiMCP::TableSetupColumn("", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGuiMCP::TableHeadersRow();

            for (const auto& npc : settings->selectedNPCs) {
                ImGuiMCP::TableNextRow();
                ImGuiMCP::TableSetColumnIndex(0);
                ImGuiMCP::TextWrapped("%s", BuildSelectedNPCLabel(npc).c_str());

                ImGuiMCP::TableSetColumnIndex(1);
                ImGuiMCP::Text("%s", npc.pluginName.empty() ? RDO_UI::GetLoc("common.unknown", "Unknown") : npc.pluginName.c_str());

                ImGuiMCP::TableSetColumnIndex(2);
                ImGuiMCP::PushID(static_cast<int>(npc.formID));
                if (ImGuiMCP::Button("X")) {
                    if (settings->RemoveSelectedNPC(npc)) {
                        changed = true;
                    }
                    ImGuiMCP::PopID();
                    break;
                }
                ImGuiMCP::PopID();
            }

            ImGuiMCP::EndTable();
        }

        return changed;
    }
}

namespace RDO_UI
{
    const char* GetLoc(const std::string& key, const char* fallback)
    {
        auto it = g_language.find(key);
        if (it != g_language.end() && !it->second.empty()) {
            return it->second.c_str();
        }
        return fallback;
    }

    void LoadLanguage()
    {
        g_language.clear();
        if (!std::filesystem::exists(LANG_PATH)) {
            return;
        }

        try {
            std::ifstream input(LANG_PATH);
            nlohmann::json document;
            input >> document;
            FlattenLanguageNode(document, "");
        }
        catch (const std::exception& e) {
            logger::warn("Falha ao carregar Language.json: {}", e.what());
        }
    }

    void RenderSettings()
    {
        auto settings = NPCSettings::GetSingleton();
        auto manager = Manager::GetSingleton();

        static bool showRestartPopup = false;
        bool changed = false;
        bool runConversion = false;

        ImGuiMCP::Text("%s", GetLoc("menu.global_configuration", "Global Configuration"));
        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();

        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, { 1.0f, 0.8f, 0.4f, 1.0f });
        ImGuiMCP::TextWrapped("%s", GetLoc(
            "menu.recommendation",
            "Recommendation: Use 'Remove Outfits' for mid-save games and if you use EDF/SPID. Use 'Full Conversion' for new games, or if you want the NPC to have the default outfit items."));
        ImGuiMCP::TextWrapped("%s", GetLoc("menu.restart_note", "NOTE: Changing modes (except from Disabled) requires a game restart."));
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::Spacing();

        const char* modes[] = {
            GetLoc("menu.mode_disabled", "Disabled (Do nothing)"),
            GetLoc("menu.mode_remove_outfits", "Remove Outfits (Safe)"),
            GetLoc("menu.mode_full_conversion", "Full Conversion (+Items)")
        };

        int currentMode = static_cast<int>(settings->outfitMode);

        ImGuiMCP::SetNextItemWidth(350.0f);
        if (ImGuiMCP::BeginCombo(GetLoc("menu.outfit_conversion_mode", "Outfit Conversion Mode"), modes[currentMode])) {
            for (int i = 0; i < 3; i++) {
                if (ImGuiMCP::Selectable(modes[i], currentMode == i)) {
                    if (currentMode != i) {
                        if (currentMode != static_cast<int>(OutfitConversionMode::kDisabled)) {
                            showRestartPopup = true;
                        }

                        settings->outfitMode = static_cast<OutfitConversionMode>(i);
                        currentMode = i;
                        changed = true;
                        runConversion = settings->outfitMode != OutfitConversionMode::kDisabled;
                    }
                }
            }
            ImGuiMCP::EndCombo();
        }

        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("%s", GetLoc(
                "menu.outfit_mode_tooltip",
                "Disabled: Does nothing.\nRemove Outfits: Only removes outfits from NPCs. Recommended for EDF/SPID.\nFull Conversion: Removes outfits and moves items to NPC inventory."));
        }

        ImGuiMCP::Spacing();
        if (ImGuiMCP::Checkbox(GetLoc("menu.enable_auto_equip", "Enable Auto Equip"), &settings->autoEquip)) {
            changed = true;
        }
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("%s", GetLoc("menu.enable_auto_equip_tooltip", "If enabled, NPCs will automatically equip the best items in their inventory."));
        }

        if (ImGuiMCP::Checkbox(GetLoc("menu.items_belong_to_npc", "Items Belong to NPC"), &settings->markAsOwned)) {
            changed = true;
        }
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("%s", GetLoc(
                "menu.items_belong_tooltip",
                "Enabled: Taking outfit items from NPC inventory has the stealing tag. Disabled: Items have no owner."));
        }

        if (ImGuiMCP::Checkbox(GetLoc("menu.remove_sleep_outfits", "Remove Sleep Outfits"), &settings->removeSleepOutfit)) {
            changed = true;
        }
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("%s", GetLoc("menu.remove_sleep_outfits_tooltip", "Also removes default sleep outfits used when NPCs go to sleep."));
        }

        ImGuiMCP::Spacing();
        const NPCTargetMode targetModes[] = {
            NPCTargetMode::kOnlyRecruitable,
            NPCTargetMode::kOnlySelected,
            NPCTargetMode::kAll
        };

        ImGuiMCP::SetNextItemWidth(300.0f);
        if (ImGuiMCP::BeginCombo(GetLoc("menu.npc_target_mode", "NPC Target Mode"), GetTargetModeLabel(settings->npcTargetMode))) {
            for (const auto mode : targetModes) {
                const bool selected = settings->npcTargetMode == mode;
                if (ImGuiMCP::Selectable(GetTargetModeLabel(mode), selected)) {
                    if (!selected) {
                        settings->npcTargetMode = mode;
                        changed = true;
                    }
                }
                if (selected) {
                    ImGuiMCP::SetItemDefaultFocus();
                }
            }
            ImGuiMCP::EndCombo();
        }
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("%s", GetLoc("menu.npc_target_mode_tooltip", "Controls which NPCs outfit removal and auto-equip are allowed to affect."));
        }

        if (settings->npcTargetMode == NPCTargetMode::kAll) {
            ImGuiMCP::Spacing();
            if (ImGuiMCP::Checkbox(GetLoc("menu.affect_children", "Children"), &settings->affectChildren)) {
                changed = true;
            }
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("%s", GetLoc("menu.affect_children_tooltip", "Allow child NPCs to be affected."));
            }

            if (ImGuiMCP::Checkbox(GetLoc("menu.affect_non_humanoid", "Non humanoid"), &settings->affectNonHumanoid)) {
                changed = true;
            }
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("%s", GetLoc("menu.affect_non_humanoid_tooltip", "Allow creatures and other non-humanoid NPCs to be affected."));
            }
        } else if (settings->npcTargetMode == NPCTargetMode::kOnlySelected) {
            ImGuiMCP::Spacing();
            if (!manager->_isPopulated) {
                manager->PopulateAllLists();
            }
            if (RenderSelectedNPCTable()) {
                changed = true;
            }
        }

        if (changed) {
            settings->Save();
            if (runConversion) {
                manager->ConvertAllNPCOutfitsToInventory();
            }
        }

        if (settings->outfitMode == OutfitConversionMode::kDisabled) {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextColored({ 1.0f, 0.4f, 0.4f, 1.0f }, "%s", GetLoc("menu.status_disabled", "Status: Plugin is currently idling (Disabled)."));
        } else {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "%s", GetLoc("menu.status_active", "Status: Outfit conversion is ACTIVE."));
        }

        if (showRestartPopup) {
            ImGuiMCP::OpenPopup(GetLoc("menu.restart_required", "Restart Required"));
        }

        if (ImGuiMCP::BeginPopupModal(GetLoc("menu.restart_required", "Restart Required"), nullptr, ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGuiMCP::Text("%s", GetLoc("menu.mode_changed", "You have changed the outfit conversion mode."));
            ImGuiMCP::Text("%s", GetLoc("menu.restart_instruction", "Please restart Skyrim to ensure the changes are applied correctly to all NPCs."));
            ImGuiMCP::Spacing();
            ImGuiMCP::Separator();
            ImGuiMCP::Spacing();

            if (ImGuiMCP::Button(GetLoc("common.ok", "OK"), { 120, 0 })) {
                showRestartPopup = false;
                ImGuiMCP::CloseCurrentPopup();
            }
            ImGuiMCP::EndPopup();
        }
    }

    void Register()
    {
        if (!SKSEMenuFramework::IsInstalled()) {
            return;
        }

        LoadLanguage();
        NPCSettings::GetSingleton()->Load();

        SKSEMenuFramework::SetSection(GetLoc("menu.section", "Remove Outfit"));
        SKSEMenuFramework::AddSectionItem(GetLoc("menu.general_settings", "General Settings"), RenderSettings);

        logger::info("RDO UI Registered successfully.");
    }
}
