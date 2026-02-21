#include "Settings.h"

namespace RDO_UI {

    void RenderSettings() {
        auto settings = NPCSettings::GetSingleton();
        auto manager = Manager::GetSingleton();

        // Estado para controlar o popup
        static bool showRestartPopup = false;

        ImGuiMCP::Text((const char*)(u8"全局设置"));
        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();

        // --- AVISO DE RECOMENDAÇÃO ---
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, { 1.0f, 0.8f, 0.4f, 1.0f });
        ImGuiMCP::TextWrapped((const char*)(u8"建议：对于存档中途的存档以及使用 EDF/SPID 的用户，请使用“移除服装”选项。\n对于新游戏，或者如果您希望 NPC 穿着默认服装，请使用“完全移除”选项。"));
        ImGuiMCP::TextWrapped((const char*)(u8"注意：更改模式（禁用模式除外）需要重新启动游戏。"));
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::Spacing();

        // --- MODO DE CONVERSÃO ---
        const char* modes[] = {
            (const char*)(u8"禁用（不执行任何操作）"),
            (const char*)(u8"移除服装（安全）"),
            (const char*)(u8"完全移除（+物品）")
        };

        int currentMode = static_cast<int>(settings->outfitMode);

        ImGuiMCP::SetNextItemWidth(350.0f);
        if (ImGuiMCP::BeginCombo((const char*)(u8"服装转换模式"), modes[currentMode])) {
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
                reinterpret_cast < const char* >(
                    u8"禁用：不执行任何操作。\n"
                    u8"移除服装：仅移除推荐用于 EDF/SPID 的 NPC 的服装。\n(如果NPC没有其他物品，他将是裸体的)。\n"
                    u8"完全移除：移除服装并将物品移至NPC物品栏。（如果未使用EDF/SPID，建议使用此功能）。"
                )
            );
        }

        // --- OUTRAS OPÇÕES ---
        ImGuiMCP::Spacing();
        if (ImGuiMCP::Checkbox((const char*)(u8"启用自动装备"), &settings->autoEquip)) {
            settings->Save();
        }
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip((const char*)(u8"启用后，NPC 将自动装备其物品栏中最好的物品。\n（如果您使用EDF/SPID，则不建议这样做）"));
        }
        if(ImGuiMCP::Checkbox((const char*)(u8"物品属于NPC"), &settings->markAsOwned)) {
            settings->Save();
        }
        if (ImGuiMCP::IsItemHovered()) {    
            ImGuiMCP::SetTooltip(
                reinterpret_cast < const char* >(
                    u8"启用：从 NPC 的物品栏中拾取服装物品会触发偷窃标签，即使是追随者也会被标记。\n"
                    u8"禁用：物品没有所有者。"
                )
            );
        }
        if (ImGuiMCP::Checkbox((const char*)(u8"移除睡衣"), &settings->removeSleepOutfit)) {
            settings->Save();
        }
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip((const char*)(u8"同时移除NPC睡觉时使用的默认睡衣。"));
        }

        // --- AVISOS DINÂMICOS ---
        if (settings->outfitMode == OutfitConversionMode::kDisabled) {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextColored({ 1.0f, 0.4f, 0.4f, 1.0f }, (const char*)(u8"状态：插件当前处于空闲状态（已禁用）。"));
        }
        else {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, (const char*)(u8"状态：服装转换功能已启用。"));
        }

        // --- LÓGICA DO POPUP DE REINICIALIZAÇÃO ---
        if (showRestartPopup) {
            ImGuiMCP::OpenPopup("Restart Required");
        }

        if (ImGuiMCP::BeginPopupModal("Restart Required", nullptr, ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGuiMCP::Text((const char*)(u8"您已更改服装转换模式。"));
            ImGuiMCP::Text((const char*)(u8"请重启游戏以确保更改正确应用于所有NPC。"));
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

        SKSEMenuFramework::SetSection((const char*)(u8"移除默认服装"));
        SKSEMenuFramework::AddSectionItem((const char*)(u8"常规设置"), RenderSettings);

        logger::info("RDO UI Registered successfully.");
    }
}