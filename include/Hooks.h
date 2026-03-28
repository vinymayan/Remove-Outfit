#include "Manager.h"
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_set>
void EquipBestInventoryItems(RE::Actor* a_actor);

inline bool IsRecruitable(RE::Actor* a_actor) {
    if (!a_actor) return false;

    auto potentialFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
    auto currentFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0001CA7D);

    bool isPotential = potentialFollowerFaction && a_actor->IsInFaction(potentialFollowerFaction);
    bool isCurrent = currentFollowerFaction && a_actor->IsInFaction(currentFollowerFaction);
    bool isTeammate = a_actor->IsPlayerTeammate();

    return isPotential || isCurrent || isTeammate;
}

class BackgroundCloneHook {
public:
    static void Install() {
        // Localiza a VTable principal de TESObjectREFR
        REL::Relocation<std::uintptr_t> vtable{ RE::Character::VTABLE[0] };

        // Realiza o hook no índice 0x6D conforme definido no header
        _ShouldBackgroundClone = vtable.write_vfunc(0x6D, &Hook_ShouldBackgroundClone);

        SKSE::log::info("Hook de ShouldBackgroundClone instalado no índice 0x6D");
    }

private:
    static bool Hook_ShouldBackgroundClone(const RE::TESObjectREFR* a_this) {
        auto actor = const_cast<RE::Actor*>(a_this->As<RE::Actor>());

        if (actor && actor != RE::PlayerCharacter::GetSingleton()) {
            auto settings = NPCSettings::GetSingleton();

            bool shouldProcess = true;
            if (settings->onlyRecruitable && !IsRecruitable(actor)) {
                shouldProcess = false;
            }

            if (settings->autoEquip) {
                // Estruturas estáticas para controlar quem já tem uma equipagem agendada
                static std::unordered_set<RE::FormID> scheduledActors;
                static std::mutex scheduledMutex;

                auto actorID = actor->GetFormID();

                std::lock_guard<std::mutex> lock(scheduledMutex);

                // Se o ator não estiver na lista de agendamentos, nós o agendamos
                if (scheduledActors.find(actorID) == scheduledActors.end()) {
                    scheduledActors.insert(actorID);

                    // Abre uma thread separada apenas para a contagem do tempo (não trava o jogo)
                    std::thread([actorID]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(400));

                        // Após 200ms, agenda a tarefa na thread principal usando a interface do SKSE
                        SKSE::GetTaskInterface()->AddTask([actorID]() {

                            // Libera o ator da lista de agendamento para permitir execuções futuras
                            {
                                std::lock_guard<std::mutex> lock(scheduledMutex);
                                scheduledActors.erase(actorID);
                            }

                            // Busca o ator com segurança pelo ID. 
                            // O ponteiro original poderia ser inválido após 200ms.
                            auto currentActor = RE::TESForm::LookupByID<RE::Actor>(actorID);
                            if (currentActor && !currentActor->IsDead()) {
                                EquipBestInventoryItems(currentActor);
                            }
                            });

                        }).detach(); // Separa a thread para rodar de forma independente
                }
            }
        }

        return _ShouldBackgroundClone(a_this);
    }

    static inline REL::Relocation<decltype(&Hook_ShouldBackgroundClone)> _ShouldBackgroundClone;
};



