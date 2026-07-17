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

// --- SISTEMA DE CONTROLE DE CONCORRENCIA ---
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

// NOVA FUNCAO: Verifica se a armadura serve na raca do ator
bool IsValidForActor(RE::TESObjectARMO* a_armor, RE::Actor* a_actor) {
    if (!a_armor || !a_actor) return false;

    RE::TESRace* race = a_actor->GetRace();
    if (!race) return false;

    // Percorre todos os ArmorAddons (ARMA) da armadura
    for (RE::TESObjectARMA* arma : a_armor->armorAddons) {
        if (!arma) continue;

        // Verifica a raca principal do Addon
        if (arma->race == race) return true;

        // Verifica as racas adicionais suportadas pelo Addon
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



    auto inventory = a_actor->GetInventory([a_actor](RE::TESBoundObject& object) {
        auto armor = object.As<RE::TESObjectARMO>();
        return armor && !armor->IsShield() && IsValidForActor(armor, a_actor);
    });

    std::map<uint32_t, BestSlotInfo> currentSlotStats;
    std::map<uint32_t, std::pair<RE::TESBoundObject*, RE::InventoryEntryData*>> bestSlotItems;
    bool modelUpdated = false;

    // 1. MAPEIA OS ITENS EQUIPAVEIS QUE JA ESTAO EM USO
    for (auto& [item, invData] : inventory) {
        auto entry = invData.second.get();
        auto armor = item ? item->As<RE::TESObjectARMO>() : nullptr;
        if (!entry || !entry->IsWorn() || !armor) {
            continue;
        }

        uint32_t mask = static_cast<uint32_t>(*armor->GetSlotMask());
        for (uint32_t i = 0; i < 32; ++i) {
            uint32_t slotBit = 1 << i;
            if (mask & slotBit) {
                BestSlotInfo info{ armor->GetArmorRating(), armor->value, armor->GetFormID() };
                if (IsBetter(armor, currentSlotStats[slotBit])) {
                    currentSlotStats[slotBit] = info;
                    bestSlotItems[slotBit] = { item, entry };
                }
            }
        }
    }

    // 2. ESCOLHE OS MELHORES ITENS POR SLOT ENTRE OS ITENS JA FILTRADOS COMO EQUIPAVEIS
    for (auto& [item, invData] : inventory) {
        auto entry = invData.second.get();
        auto armor = item ? item->As<RE::TESObjectARMO>() : nullptr;
        if (invData.first <= 0 || !entry || !armor) {
            continue;
        }

        uint32_t mask = static_cast<uint32_t>(*armor->GetSlotMask());

        for (uint32_t i = 0; i < 32; ++i) {
            uint32_t slotBit = 1 << i;
            if ((mask & slotBit) && IsBetter(armor, currentSlotStats[slotBit])) {
                currentSlotStats[slotBit] = { armor->GetArmorRating(), armor->value, armor->GetFormID() };
                bestSlotItems[slotBit] = { item, entry };
            }
        }
    }

    // 3. EQUIPA CADA ITEM VENCEDOR UMA UNICA VEZ
    std::set<RE::FormID> equippedForms;
    for (auto& [slot, bestItem] : bestSlotItems) {
        auto [item, entry] = bestItem;
        if (!item || !entry || entry->IsWorn()) {
            continue;
        }

        if (!equippedForms.insert(item->GetFormID()).second) {
            continue;
        }

        auto extraData = (entry->extraLists && !entry->extraLists->empty()) ? entry->extraLists->front() : nullptr;
        equipManager->EquipObject(a_actor, item, extraData, 1, nullptr, false, true, false, true);
        modelUpdated = true;
    }

    if (modelUpdated) {
        a_actor->Update3DModel();
    }
}
