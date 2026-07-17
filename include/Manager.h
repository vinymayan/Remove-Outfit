#pragma once

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "ClibUtil/editorID.hpp"
#include <nlohmann/json.hpp>



struct InternalFormInfo {
    RE::FormID formID;
    std::string editorID;
    std::string name;
    std::string pluginName;
    std::string formType;

    // Helper for UI
    std::string GetDisplayName() const {
        if (!name.empty()) return name;
        if (!editorID.empty()) return editorID;
        return std::to_string(formID);
    }
};

enum class OutfitConversionMode : int {
    kDisabled = 0,
    kOnlyEmpty = 1,     // Ligado (Apenas remove)
    kFullConversion = 2 // Ligado (Converte para inventario e adiciona)
    
};

enum class NPCTargetMode : int {
    kOnlyRecruitable = 0,
    kOnlySelected = 1,
    kAll = 2
};

struct SelectedNPCInfo {
    RE::FormID formID = 0;
    RE::FormID localFormID = 0;
    std::string pluginName;
    std::string displayName;
};

class NPCSettings {
public:
    static NPCSettings* GetSingleton() {
        static NPCSettings singleton;
        return &singleton;
    }

    // Valor padrao inicial
    OutfitConversionMode outfitMode = OutfitConversionMode::kFullConversion;
    bool removeSleepOutfit = false;
    bool autoEquip = true;
    bool markAsOwned = false;
    bool affectChildren = false;
    bool affectNonHumanoid = false;
    NPCTargetMode npcTargetMode = NPCTargetMode::kAll;
    std::vector<SelectedNPCInfo> selectedNPCs;

    void Load() {
        const auto loadPath = std::filesystem::exists(settingsPath) ? settingsPath : legacySettingsPath;
        if (!std::filesystem::exists(loadPath)) {
            Save(); // Cria o padrao se nao existir
            return;
        }

        try {
            std::ifstream i(loadPath);
            nlohmann::json j;
            i >> j;
            outfitMode = static_cast<OutfitConversionMode>(j.value("outfitMode", static_cast<int>(outfitMode)));
            removeSleepOutfit = j.value("removeSleepOutfit", false);
            autoEquip = j.value("autoEquip", true);
            markAsOwned = j.value("markAsOwned", false);
            affectChildren = j.value("affectChildren", false);
            affectNonHumanoid = j.value("affectNonHumanoid", false);

            if (j.contains("npcTargetMode") && j["npcTargetMode"].is_number_integer()) {
                npcTargetMode = static_cast<NPCTargetMode>(j["npcTargetMode"].get<int>());
            } else {
                npcTargetMode = j.value("onlyRecruitable", false) ? NPCTargetMode::kOnlyRecruitable : NPCTargetMode::kAll;
            }

            selectedNPCs.clear();
            if (j.contains("selectedNPCs") && j["selectedNPCs"].is_array()) {
                for (const auto& entry : j["selectedNPCs"]) {
                    if (!entry.is_object()) {
                        continue;
                    }

                    SelectedNPCInfo npc;
                    npc.formID = entry.value("formID", 0u);
                    npc.localFormID = entry.value("localFormID", 0u);
                    npc.pluginName = entry.value("pluginName", "");
                    npc.displayName = entry.value("displayName", "");
                    if (npc.formID != 0 || (!npc.pluginName.empty() && npc.localFormID != 0)) {
                        selectedNPCs.push_back(npc);
                    }
                }
            }

            Clamp();

            if (loadPath == legacySettingsPath) {
                Save();
            }
        }
        catch (const std::exception& e) {
            SKSE::log::error("Falha ao carregar settings: {}", e.what());
        }
    }

    void Save() {
        try {
            std::filesystem::create_directories(std::filesystem::path(settingsPath).parent_path());
            nlohmann::json j;
            j["outfitMode"] = static_cast<int>(outfitMode);
            j["removeSleepOutfit"] = removeSleepOutfit;
            j["autoEquip"] = autoEquip;
            j["markAsOwned"] = markAsOwned;
            j["affectChildren"] = affectChildren;
            j["affectNonHumanoid"] = affectNonHumanoid;
            j["npcTargetMode"] = static_cast<int>(npcTargetMode);

            j["selectedNPCs"] = nlohmann::json::array();
            for (const auto& npc : selectedNPCs) {
                j["selectedNPCs"].push_back({
                    { "formID", npc.formID },
                    { "localFormID", npc.localFormID },
                    { "pluginName", npc.pluginName },
                    { "displayName", npc.displayName }
                });
            }

            std::ofstream o(settingsPath);
            o << j.dump(4);
        }
        catch (const std::exception& e) {
            SKSE::log::error("Falha ao salvar settings: {}", e.what());
        }
    }

    bool AddSelectedNPC(RE::TESNPC* npc) {
        if (!npc || IsSelectedNPC(npc)) {
            return false;
        }

        SelectedNPCInfo info;
        info.formID = npc->GetFormID();
        info.localFormID = GetLocalFormID(npc);
        info.displayName = BuildDisplayName(npc);

        if (auto file = npc->GetFile(0)) {
            info.pluginName = file->GetFilename();
        }

        selectedNPCs.push_back(info);
        return true;
    }

    bool RemoveSelectedNPC(const SelectedNPCInfo& selected) {
        const auto oldSize = selectedNPCs.size();
        std::erase_if(selectedNPCs, [&](const auto& npc) {
            return MatchesSelectedEntry(npc, selected);
        });
        return selectedNPCs.size() != oldSize;
    }

    bool IsSelectedNPC(RE::TESNPC* npc) const {
        if (!npc) {
            return false;
        }

        return std::ranges::any_of(selectedNPCs, [&](const auto& selected) {
            return MatchesForm(npc, selected);
        });
    }

    bool ShouldAffectNPC(RE::TESNPC* npc) const {
        if (!npc) {
            return false;
        }

        switch (npcTargetMode) {
        case NPCTargetMode::kOnlyRecruitable:
            return IsRecruitableNPC(npc);
        case NPCTargetMode::kOnlySelected:
            return IsSelectedNPC(npc);
        case NPCTargetMode::kAll:
        default:
            return PassesAllNPCFilters(npc->GetRace());
        }
    }

    bool ShouldAffectActor(RE::Actor* actor) const {
        if (!actor) {
            return false;
        }

        switch (npcTargetMode) {
        case NPCTargetMode::kOnlyRecruitable:
            return IsRecruitableActor(actor);
        case NPCTargetMode::kOnlySelected:
            return IsSelectedNPC(actor->GetActorBase());
        case NPCTargetMode::kAll:
        default:
            return PassesAllNPCFilters(actor->GetRace());
        }
    }

private:
    const std::string settingsPath = "Data/Viny Mods/Remove Outfit/Settings.json";
    const std::string legacySettingsPath = "Data/SKSE/Plugins/RDO_Settings.json";

    void Clamp() {
        const auto mode = static_cast<int>(outfitMode);
        if (mode < static_cast<int>(OutfitConversionMode::kDisabled) || mode > static_cast<int>(OutfitConversionMode::kFullConversion)) {
            outfitMode = OutfitConversionMode::kFullConversion;
        }

        const auto targetMode = static_cast<int>(npcTargetMode);
        if (targetMode < static_cast<int>(NPCTargetMode::kOnlyRecruitable) || targetMode > static_cast<int>(NPCTargetMode::kAll)) {
            npcTargetMode = NPCTargetMode::kAll;
        }
    }

    static bool IsRecruitableNPC(RE::TESNPC* npc) {
        if (!npc) {
            return false;
        }

        auto potentialFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
        auto currentFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0001CA7D);

        return (potentialFollowerFaction && npc->IsInFaction(potentialFollowerFaction)) ||
            (currentFollowerFaction && npc->IsInFaction(currentFollowerFaction));
    }

    static bool IsRecruitableActor(RE::Actor* actor) {
        if (!actor) {
            return false;
        }

        auto potentialFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
        auto currentFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0001CA7D);

        return (potentialFollowerFaction && actor->IsInFaction(potentialFollowerFaction)) ||
            (currentFollowerFaction && actor->IsInFaction(currentFollowerFaction)) ||
            actor->IsPlayerTeammate();
    }

    bool PassesAllNPCFilters(RE::TESRace* race) const {
        if (!race) {
            return affectNonHumanoid;
        }

        if (!affectChildren && race->IsChildRace()) {
            return false;
        }

        static const auto actorTypeNPC = RE::TESForm::LookupByID<RE::BGSKeyword>(0x00013794);
        const bool isHumanoid = actorTypeNPC && race->HasKeyword(actorTypeNPC);
        return affectNonHumanoid || isHumanoid;
    }

    static RE::FormID GetLocalFormID(RE::TESForm* form) {
        if (!form) {
            return 0;
        }

        auto file = form->GetFile(0);
        if (file && file->IsLight()) {
            return form->GetFormID() & 0xFFF;
        }

        return form->GetFormID() & 0xFFFFFF;
    }

    static std::string BuildDisplayName(RE::TESNPC* npc) {
        if (!npc) {
            return {};
        }

        if (const auto name = npc->fullName.c_str(); name && name[0] != '\0') {
            return name;
        }

        auto editorID = clib_util::editorID::get_editorID(npc);
        if (!editorID.empty()) {
            return editorID;
        }

        return std::format("{:08X}", npc->GetFormID());
    }

    static bool MatchesForm(RE::TESNPC* npc, const SelectedNPCInfo& selected) {
        if (!npc) {
            return false;
        }

        if (selected.formID != 0 && selected.formID == npc->GetFormID()) {
            return true;
        }

        if (selected.pluginName.empty() || selected.localFormID == 0) {
            return false;
        }

        auto file = npc->GetFile(0);
        return file &&
            selected.pluginName == file->GetFilename() &&
            selected.localFormID == GetLocalFormID(npc);
    }

    static bool MatchesSelectedEntry(const SelectedNPCInfo& lhs, const SelectedNPCInfo& rhs) {
        if (lhs.formID != 0 && rhs.formID != 0 && lhs.formID == rhs.formID) {
            return true;
        }

        return !lhs.pluginName.empty() &&
            !rhs.pluginName.empty() &&
            lhs.pluginName == rhs.pluginName &&
            lhs.localFormID != 0 &&
            lhs.localFormID == rhs.localFormID;
    }
};

class Manager {
public:
    static Manager* GetSingleton() {
        static Manager singleton;
        return &singleton;
    }

    void PopulateAllLists();

    // Data Store: Map of "TypeName" -> List of InternalFormInfo
    // We use this to feed the UI
    const std::vector<InternalFormInfo>& GetList(const std::string& typeName);

    // Register callback for when population is done
    void RegisterReadyCallback(std::function<void()> callback);

    void ConvertAllNPCOutfitsToInventory();


    bool _isPopulated = false;

private:
    Manager() = default;

    template <typename T>
    void PopulateList(const std::string& a_typeName);
    std::map<std::string, std::vector<InternalFormInfo>> _dataStore;
    std::vector<std::function<void()>> _readyCallbacks;
};
