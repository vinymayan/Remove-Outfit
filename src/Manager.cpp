#include "Manager.h"


void Manager::PopulateAllLists() {
    if (_isPopulated) return;

    logger::info("Iniciando escaneamento de FormTypes...");
    PopulateList<RE::TESNPC>("NPC");


    _isPopulated = true;
    for (auto cb : _readyCallbacks) {
        if (cb) cb();
    }
    _readyCallbacks.clear();
}

const std::vector<InternalFormInfo>& Manager::GetList(const std::string& typeName) {
    static std::vector<InternalFormInfo> empty;
    auto it = _dataStore.find(typeName);
    if (it != _dataStore.end()) {
        return it->second;
    }
    return empty;
}

void Manager::RegisterReadyCallback(std::function<void()> callback) {
    if (_isPopulated) {
        callback();
    }
    else {
        _readyCallbacks.push_back(callback);
    }
}

template <typename T>
void Manager::PopulateList(const std::string& a_typeName) {
    auto dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) return;

    auto& list = _dataStore[a_typeName];
    list.clear();

    const auto& forms = dataHandler->GetFormArray<T>();
    list.reserve(forms.size());

    for (const auto& form : forms) {
        if (!form) continue;

        InternalFormInfo info;
        info.formID = form->GetFormID();
        info.formType = a_typeName;

        // Atribuição segura de string
        // editorID pode não existir em build pública, mas clib_util tenta pegar
        info.editorID = clib_util::editorID::get_editorID(form);

        info.name = "";

        // 1. Verifica se o form é um NPC
        if (form->Is(RE::FormType::NPC)) {
            if (auto npc = form->As<RE::TESNPC>()) {
                info.name = npc->fullName.c_str();
            }
        }
        // 2. Opcional: Se não for NPC, tenta pegar o nome de qualquer objeto que tenha TESFullName (Spells, Itens, etc)
        else if (auto fullName = form->As<RE::TESFullName>()) {
            info.name = fullName->fullName.c_str();
        }

        if (auto file = form->GetFile(0)) {
            info.pluginName = file->GetFilename();
        }
        else {
            info.pluginName = "Dynamic"; // Created at runtime
        }

        list.push_back(info);
    }
    logger::info("Carregados {} itens do tipo {}", list.size(), a_typeName);
}

void Manager::ConvertAllNPCOutfitsToInventory() {
    auto settings = NPCSettings::GetSingleton();
    if (settings->outfitMode == OutfitConversionMode::kDisabled) {
        logger::info("[Outfit] Conversão desativada nas configurações.");
        return;
    }

    auto dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) return;

    auto potentialFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
    auto currentFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0001CA7D);

    const auto& npcArray = dataHandler->GetFormArray<RE::TESNPC>();
    uint32_t count = 0;

    for (auto* npc : npcArray) {
        if (!npc) continue;

        if (settings->onlyRecruitable) {
            bool isPotential = potentialFollowerFaction && npc->IsInFaction(potentialFollowerFaction);
            bool isCurrent = currentFollowerFaction && npc->IsInFaction(currentFollowerFaction);

            if (!isPotential && !isCurrent) {
                continue; // Pula este NPC e vai para o próximo
            }
        }

        RE::TESForm* itemOwner = settings->markAsOwned ? npc : nullptr;
        bool modified = false;
        // 1. Processamento do Default Outfit: Verifica explicitamente se NÃO é nullptr
        if (npc->defaultOutfit != nullptr) {
            // Se for Full Conversion, move os itens para o inventário antes de limpar
            if (settings->outfitMode == OutfitConversionMode::kFullConversion) {
                for (auto* item : npc->defaultOutfit->outfitItems) {
                    if (item) {
                        auto* boundItem = item->As<RE::TESBoundObject>();
                        if (boundItem && npc->GetObjectCount(boundItem) == 0) {
                            npc->AddObjectToContainer(boundItem, 1, itemOwner);
                        }
                    }
                }
            }

            // Em ambos os modos (Safe ou Full), o ponteiro é limpo se existia algo ali
            npc->defaultOutfit = nullptr;
            modified = true;
			logger::debug("[Outfit] Default Outfit removido de '{}'.", npc->GetName());
        }

        // 2. Processamento do Sleep Outfit: Verifica se a opção está ativa e se o outfit existe
        if (settings->removeSleepOutfit && npc->sleepOutfit != nullptr) {
            if (settings->outfitMode == OutfitConversionMode::kFullConversion) {
                for (auto* item : npc->sleepOutfit->outfitItems) {
                    if (item) {
                        auto* boundItem = item->As<RE::TESBoundObject>();
                        if (boundItem && npc->GetObjectCount(boundItem) == 0) {
                            npc->AddObjectToContainer(boundItem, 1, itemOwner);
                        }
                    }
                }
            }

            npc->sleepOutfit = nullptr;
            modified = true;
            logger::debug("[Outfit] Sleep Outfit removido de '{}'.", npc->GetName());
        }

        if (modified) count++;
    }
    logger::info("Processados {} NPCs: Outfits convertidos em itens de inventário.", count);
}