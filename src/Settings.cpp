#include "Settings.h"

namespace RDO_UI {

    void RenderSettings() {
        auto settings = NPCSettings::GetSingleton();
        auto manager = Manager::GetSingleton();

        // Estado para controlar o popup
        static bool showRestartPopup = false;

        ImGuiMCP::Text("Global Configuration");
        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();

        // --- AVISO DE RECOMENDAÇÃO ---
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, { 1.0f, 0.8f, 0.4f, 1.0f });
        ImGuiMCP::TextWrapped("Recommendation: Use 'Remove Outfits' for mid-save games and if you use EDF/SPID. \nUse 'Full Conversion' for new games, or if want the NPC to have the default outfit items");
        ImGuiMCP::TextWrapped("NOTE: Changing modes (except from Disabled) requires a game restart.");
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::Spacing();

        // --- MODO DE CONVERSÃO ---
        const char* modes[] = {
            "Disabled (Do nothing)",
            "Remove Outfits (Safe)",
            "Full Conversion (+Items)"
        };

        int currentMode = static_cast<int>(settings->outfitMode);

        ImGuiMCP::SetNextItemWidth(350.0f);
        if (ImGuiMCP::BeginCombo("Outfit Conversion Mode", modes[currentMode])) {
            for (int i = 0; i < 3; i++) {
                if (ImGuiMCP::Selectable(modes[i], currentMode == i)) {
                    if (currentMode != i) {
                        // Lógica: Se sair de qualquer modo que NÃO seja Disable (0), mostra o popup
                        if (currentMode != static_cast<int>(OutfitConversionMode::kDisabled)) {
                            showRestartPopup = true;
                        }

                        settings->outfitMode = static_cast<OutfitConversionMode>(i);
                        settings->Save();
                        if (settings->outfitMode != OutfitConversionMode::kDisabled) {
                            manager->ConvertAllNPCOutfitsToInventory();
                        }
                    }
                }
            }
            ImGuiMCP::EndCombo();
        }

        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip(
                "Disabled: Does nothing.\n"
                "Remove Outfits: Only removes outfits from NPCs recommended for EDF/SPID \n(If the NPC doesnt have other items it will be naked).\n"
                "Full Conversion: Removes outfits and moves items to NPC inventory. (Recommended if NOT using EDF/SPID)."
            );
        }

        // --- OUTRAS OPÇÕES ---
        ImGuiMCP::Spacing();
        if (ImGuiMCP::Checkbox("Enable Auto Equip", &settings->autoEquip)) {
            settings->Save();
        }
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("If enabled, NPCs will automatically equip the best items in their inventory.\n(Not recommended if you use EDF/SPID)");
        }
        if(ImGuiMCP::Checkbox("Items Belong to NPC", &settings->markAsOwned)) {
            settings->Save();
        }
        if (ImGuiMCP::IsItemHovered()) {    
            ImGuiMCP::SetTooltip(
                "Enabled: Taking outfit items from NPC's inventory has stealing tag even followers.\n"
                "Disabled: Items have no owner."
            );
        }
        if (ImGuiMCP::Checkbox("Remove Sleep Outfits", &settings->removeSleepOutfit)) {
            settings->Save();
        }
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("Also removes default sleep outfits used when NPCs go to sleep.");
        }

        // --- AVISOS DINÂMICOS ---
        if (settings->outfitMode == OutfitConversionMode::kDisabled) {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextColored({ 1.0f, 0.4f, 0.4f, 1.0f }, "Status: Plugin is currently idling (Disabled).");
        }
        else {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "Status: Outfit conversion is ACTIVE.");
        }

        // --- LÓGICA DO POPUP DE REINICIALIZAÇÃO ---
        if (showRestartPopup) {
            ImGuiMCP::OpenPopup("Restart Required");
        }

        if (ImGuiMCP::BeginPopupModal("Restart Required", nullptr, ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGuiMCP::Text("You have changed the outfit conversion mode.");
            ImGuiMCP::Text("Please restart Skyrim to ensure the changes are applied correctly to all NPCs.");
            ImGuiMCP::Spacing();
            ImGuiMCP::Separator();
            ImGuiMCP::Spacing();

            if (ImGuiMCP::Button("OK", { 120, 0 })) {
                showRestartPopup = false;
                ImGuiMCP::CloseCurrentPopup();
            }
            ImGuiMCP::EndPopup();
        }
    }

    void Register() {
        if (!SKSEMenuFramework::IsInstalled()) {
            return;
        }

        SKSEMenuFramework::SetSection("Remove Default Outfit");
        SKSEMenuFramework::AddSectionItem("General Settings", RenderSettings);

        logger::info("RDO UI Registered successfully.");
    }
}