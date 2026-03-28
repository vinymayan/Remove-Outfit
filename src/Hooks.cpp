#include "Hooks.h"
#include <mutex>
#include <set>
#include <map>
#include <vector>
#include <algorithm>

struct BestSlotInfo {
    float rating = -1.0f;
    int32_t value = -1;
    RE::FormID formID = 0;
};

// --- SISTEMA DE CONTROLE DE CONCORRÊNCIA ---
static std::mutex g_equipBestMutex;
static std::set<RE::FormID> g_actorsInEquipBest;

bool IsBetter(RE::TESObjectARMO* a_candidate, const BestSlotInfo& a_current) {
    float candidateRating = a_candidate->GetArmorRating();
    int32_t candidateValue = a_candidate->value; // Herdado de TESValueForm
    RE::FormID candidateID = a_candidate->GetFormID();

    if (candidateRating > a_current.rating) return true;
    if (candidateRating < a_current.rating) return false;

    if (candidateValue > a_current.value) return true;
    if (candidateValue < a_current.value) return false;

    return candidateID > a_current.formID;
}

// NOVA FUNÇÃO: Verifica se a armadura serve na raça do ator
bool IsValidForActor(RE::TESObjectARMO* a_armor, RE::Actor* a_actor) {
    if (!a_armor || !a_actor) return false;

    RE::TESRace* race = a_actor->GetRace();
    if (!race) return false;

    // Percorre todos os ArmorAddons (ARMA) da armadura
    for (RE::TESObjectARMA* arma : a_armor->armorAddons) {
        if (!arma) continue;

        // Verifica a raça principal do Addon
        if (arma->race == race) return true;

        // Verifica as raças adicionais suportadas pelo Addon
        for (RE::TESRace* addRace : arma->additionalRaces) {
            if (addRace == race) return true;
        }
    }

    return false;
}

void EquipBestInventoryItems(RE::Actor* a_actor)
{
    if (!a_actor) return;

    RE::FormID actorID = a_actor->GetFormID();

    {
        std::lock_guard<std::mutex> lock(g_equipBestMutex);
        if (g_actorsInEquipBest.contains(actorID)) return;
        g_actorsInEquipBest.insert(actorID);
    }

    struct EquipCleaner {
        RE::FormID id;
        ~EquipCleaner() {
            std::lock_guard<std::mutex> lock(g_equipBestMutex);
            g_actorsInEquipBest.erase(id);
        }
    } cleaner{ actorID };

    auto equipManager = RE::ActorEquipManager::GetSingleton();
    if (!equipManager) return;



    auto inventory = a_actor->GetInventory();
    std::map<uint32_t, BestSlotInfo> currentSlotStats;
    bool modelUpdated = false;

    // 1. MAPEAMENTO E CORREÇÃO DE SLOTS ATUAIS
    for (auto& [item, invData] : inventory) {
        if (invData.second && invData.second->IsWorn() && item->IsArmor()) {
            auto armor = item->As<RE::TESObjectARMO>();

            // Se for criança e a armadura atual não servir (ex: Ebony Armor), desequipa imediatamente
            if (!IsValidForActor(armor, a_actor)) {
                equipManager->UnequipObject(a_actor, item, nullptr, 1, nullptr, false, false, false);
                modelUpdated = true;
                continue;
            }

            uint32_t mask = static_cast<uint32_t>(*armor->GetSlotMask());
            for (uint32_t i = 0; i < 32; ++i) {
                uint32_t slotBit = 1 << i;
                if (mask & slotBit) {
                    BestSlotInfo info{ armor->GetArmorRating(), armor->value, armor->GetFormID() };
                    if (info.rating > currentSlotStats[slotBit].rating) {
                        currentSlotStats[slotBit] = info;
                    }
                }
            }
        }
    }

    // 2. COLETA E ORDENAÇÃO
    using InventoryPair = std::pair<RE::TESBoundObject*, RE::InventoryEntryData*>;
    std::vector<InventoryPair> itemsToProcess;

    for (auto& [item, invData] : inventory) {
        if (invData.first > 0 && item->IsArmor()) {
            auto armor = item->As<RE::TESObjectARMO>();
            if (armor && !armor->IsShield()) {

                if (!IsValidForActor(armor, a_actor)) {
                    continue;
                }

                itemsToProcess.push_back({ item, invData.second.get() });
            }
        }
    }

    std::sort(itemsToProcess.begin(), itemsToProcess.end(), [](const InventoryPair& a, const InventoryPair& b) {
        auto armoA = a.first->As<RE::TESObjectARMO>();
        auto armoB = b.first->As<RE::TESObjectARMO>();
        if (armoA->GetArmorRating() != armoB->GetArmorRating()) return armoA->GetArmorRating() > armoB->GetArmorRating();
        if (armoA->value != armoB->value) return armoA->value > armoB->value;
        return armoA->GetFormID() > armoB->GetFormID();
        });

    // 3. EQUIPAGEM SELETIVA
    for (auto& [item, entry] : itemsToProcess) {
        auto armor = item->As<RE::TESObjectARMO>();
        uint32_t mask = static_cast<uint32_t>(*armor->GetSlotMask());

        bool isBetterForAnySlot = false;
        for (uint32_t i = 0; i < 32; ++i) {
            uint32_t slotBit = 1 << i;
            if (mask & slotBit) {
                if (IsBetter(armor, currentSlotStats[slotBit])) {
                    isBetterForAnySlot = true;
                    break;
                }
            }
        }

        if (isBetterForAnySlot && !entry->IsWorn()) {
            auto extraData = (entry->extraLists && !entry->extraLists->empty()) ? entry->extraLists->front() : nullptr;
            equipManager->EquipObject(a_actor, item, extraData, 1, nullptr, false, true, false, true);
            modelUpdated = true;

            for (uint32_t i = 0; i < 32; ++i) {
                uint32_t slotBit = 1 << i;
                if (mask & slotBit) {
                    currentSlotStats[slotBit] = { armor->GetArmorRating(), armor->value, armor->GetFormID() };
                }
            }
        }
    }

    if (modelUpdated) {
        a_actor->Update3DModel();
    }
}