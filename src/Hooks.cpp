#include "Hooks.h"
#include <mutex>
#include <set>

// --- SISTEMA DE CONTROLE DE CONCORRÊNCIA ---
static std::mutex g_equipBestMutex;
static std::set<RE::FormID> g_actorsInEquipBest;

void EquipBestInventoryItems(RE::Actor* a_actor)
{
    if (!a_actor) return;

    RE::FormID actorID = a_actor->GetFormID();

    // 0. BLOQUEIO DE CONCORRÊNCIA
    {
        std::lock_guard<std::mutex> lock(g_equipBestMutex);
        if (g_actorsInEquipBest.contains(actorID)) {
            // Já existe uma thread calculando o melhor equipamento para este ator
            return;
        }
        g_actorsInEquipBest.insert(actorID);
    }

    // Limpeza automática ao sair da função
    struct EquipCleaner {
        RE::FormID id;
        ~EquipCleaner() {
            std::lock_guard<std::mutex> lock(g_equipBestMutex);
            g_actorsInEquipBest.erase(id);
        }
    } cleaner{ actorID };

    auto equipManager = RE::ActorEquipManager::GetSingleton();
    if (!equipManager) return;

    std::string actorName = a_actor->GetName();
    logger::debug("[EquipBest] Iniciando reavaliação de equipamento para {}", actorName);

    auto inventory = a_actor->GetInventory();

    // 1. MAPEAMENTO DE SLOTS ATUAIS
    std::map<uint32_t, float> currentSlotRatings;

    for (auto& [item, invData] : inventory) {
        if (invData.second && invData.second->IsWorn() && item->IsArmor()) {
            auto armor = item->As<RE::TESObjectARMO>();
            uint32_t mask = static_cast<uint32_t>(armor->GetSlotMask());
            float rating = armor->GetArmorRating();

            for (uint32_t i = 0; i < 32; ++i) {
                uint32_t slotBit = 1 << i;
                if (mask & slotBit) {
                    if (currentSlotRatings.find(slotBit) == currentSlotRatings.end() || rating > currentSlotRatings[slotBit]) {
                        currentSlotRatings[slotBit] = rating;
                    }
                }
            }
        }
    }

    // 2. COLETA E ORDENAÇÃO
    using InventoryPair = std::pair<RE::TESBoundObject*, RE::InventoryEntryData*>;
    std::vector<InventoryPair> itemsToProcess;

    for (auto& [item, invData] : inventory) {
        auto& [count, entry] = invData;
        if (count > 0 && item->IsArmor()) {
            auto armor = item->As<RE::TESObjectARMO>();
            if (armor && !armor->IsShield()) {
                itemsToProcess.push_back({ item, entry.get() });
            }
        }
    }

    std::sort(itemsToProcess.begin(), itemsToProcess.end(), [](const InventoryPair& a, const InventoryPair& b) {
        return a.first->As<RE::TESObjectARMO>()->GetArmorRating() > b.first->As<RE::TESObjectARMO>()->GetArmorRating();
        });

    // 3. EQUIPAGEM SELETIVA
    bool modelUpdated = false;
    for (auto& [item, entry] : itemsToProcess) {
        auto armor = item->As<RE::TESObjectARMO>();
        uint32_t mask = static_cast<uint32_t>(armor->GetSlotMask());
        float candidateRating = armor->GetArmorRating();

        bool shouldEquip = false;

        for (uint32_t i = 0; i < 32; ++i) {
            uint32_t slotBit = 1 << i;
            if (mask & slotBit) {
                auto it = currentSlotRatings.find(slotBit);
                if (it == currentSlotRatings.end() || candidateRating > it->second) {
                    shouldEquip = true;
                    break;
                }
            }
        }

        if (shouldEquip && !entry->IsWorn()) {
            auto extraData = (entry->extraLists && !entry->extraLists->empty()) ? entry->extraLists->front() : nullptr;

            logger::debug("  [EquipBest] Executando: Equipar '{}' (Rating: {}) em {}", item->GetName(), candidateRating, actorName);

            // Note: force=true (penúltimo parâmetro) para garantir a troca
            equipManager->EquipObject(a_actor, item, extraData, 1, nullptr, false, true, false, true);
            modelUpdated = true;

            for (uint32_t i = 0; i < 32; ++i) {
                uint32_t slotBit = 1 << i;
                if (mask & slotBit) {
                    currentSlotRatings[slotBit] = candidateRating;
                }
            }
        }
    }

    if (modelUpdated) {
        a_actor->Update3DModel();
        logger::debug("[EquipBest] Processamento concluído e modelo 3D atualizado para {}", actorName);
    }
}