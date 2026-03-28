#pragma once

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
    kFullConversion = 2 // Ligado (Converte para inventário e adiciona)
    
};

class NPCSettings {
public:
    static NPCSettings* GetSingleton() {
        static NPCSettings singleton;
        return &singleton;
    }

    // Valor padrão inicial
    OutfitConversionMode outfitMode = OutfitConversionMode::kFullConversion;
    bool removeSleepOutfit = false;
    bool autoEquip = true;
    bool markAsOwned = false;
    bool onlyRecruitable = false;

    void Load() {
        if (!std::filesystem::exists(settingsPath)) {
            Save(); // Cria o padrão se não existir
            return;
        }

        try {
            std::ifstream i(settingsPath);
            nlohmann::json j;
            i >> j;
            outfitMode = static_cast<OutfitConversionMode>(j.value("outfitMode", (int)outfitMode));
            removeSleepOutfit = j.value("removeSleepOutfit", false);
            autoEquip = j.value("autoEquip", true);
            markAsOwned = j.value("markAsOwned", false);
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
            std::ofstream o(settingsPath);
            o << j.dump(4);
        }
        catch (const std::exception& e) {
            SKSE::log::error("Falha ao salvar settings: {}", e.what());
        }
    }

private:
    const std::string settingsPath = "Data/SKSE/Plugins/RDO_Settings.json";
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

private:
    Manager() = default;

    template <typename T>
    void PopulateList(const std::string& a_typeName);

    bool _isPopulated = false;
    std::map<std::string, std::vector<InternalFormInfo>> _dataStore;
    std::vector<std::function<void()>> _readyCallbacks;
};
