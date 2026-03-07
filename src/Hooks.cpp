#include "Hooks.h"
#include <mutex>
#include <set>

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

    // 1. Prioridade: Rating de Armadura
    if (candidateRating > a_current.rating) return true;
    if (candidateRating < a_current.rating) return false;

    // 2. Desempate: Valor (Prioriza itens como 'Robins Clothes')
    if (candidateValue > a_current.value) return true;
    if (candidateValue < a_current.value) return false;

    // 3. Estabilidade Final: FormID (Evita loops infinitos)
    return candidateID > a_current.formID;
}

void EquipBestInventoryItems(RE::Actor* a_actor)
{
    if (!a_actor) return;

    RE::FormID actorID = a_actor->GetFormID();

    // 0. BLOQUEIO DE CONCORRÊNCIA
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

    // 1. MAPEAMENTO DE SLOTS ATUAIS
    std::map<uint32_t, BestSlotInfo> currentSlotStats;

    for (auto& [item, invData] : inventory) {
        if (invData.second && invData.second->IsWorn() && item->IsArmor()) {
            auto armor = item->As<RE::TESObjectARMO>();
            uint32_t mask = static_cast<uint32_t>(*armor->GetSlotMask());

            for (uint32_t i = 0; i < 32; ++i) {
                uint32_t slotBit = 1 << i;
                if (mask & slotBit) {
                    BestSlotInfo info{ armor->GetArmorRating(), armor->value, armor->GetFormID() };
                    // Se o item atual é melhor que o registrado para o slot (em caso de itens sobrepostos)
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
            // Ignora escudos para não interferir com armas
            if (armor && !armor->IsShield()) {
                itemsToProcess.push_back({ item, invData.second.get() });
            }
        }
    }

    // Ordenação idêntica à lógica de comparação
    std::sort(itemsToProcess.begin(), itemsToProcess.end(), [](const InventoryPair& a, const InventoryPair& b) {
        auto armoA = a.first->As<RE::TESObjectARMO>();
        auto armoB = b.first->As<RE::TESObjectARMO>();
        if (armoA->GetArmorRating() != armoB->GetArmorRating()) return armoA->GetArmorRating() > armoB->GetArmorRating();
        if (armoA->value != armoB->value) return armoA->value > armoB->value;
        return armoA->GetFormID() > armoB->GetFormID();
        });

    // 3. EQUIPAGEM SELETIVA
    bool modelUpdated = false;
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

            // EquipObject com force=true e preventUnequip=false
            equipManager->EquipObject(a_actor, item, extraData, 1, nullptr, false, true, false, true);
            modelUpdated = true;

            // Atualiza o mapa de slots para os próximos itens da lista não tentarem equipar por cima
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