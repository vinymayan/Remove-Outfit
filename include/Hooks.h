#include "Manager.h"
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_set>
void EquipBestInventoryItems(RE::Actor* a_actor);

class BackgroundCloneHook {
public:
    static void Install() {
        // Localiza a VTable principal de TESObjectREFR
        REL::Relocation<std::uintptr_t> vtable{ RE::Character::VTABLE[0] };

        // Realiza o hook no indice 0x6D conforme definido no header
        _ShouldBackgroundClone = vtable.write_vfunc(0x6D, &Hook_ShouldBackgroundClone);

        SKSE::log::info("Hook de ShouldBackgroundClone instalado no indice 0x6D");
    }

private:
    static bool Hook_ShouldBackgroundClone(const RE::TESObjectREFR* a_this) {
        auto actor = const_cast<RE::Actor*>(a_this->As<RE::Actor>());

        if (actor && actor != RE::PlayerCharacter::GetSingleton()) {
            auto settings = NPCSettings::GetSingleton();

            if (settings->autoEquip && settings->ShouldAffectActor(actor)) {
                // Estruturas estaticas para controlar quem ja tem uma equipagem agendada
                static std::unordered_set<RE::FormID> scheduledActors;
                static std::mutex scheduledMutex;

                auto actorID = actor->GetFormID();

                std::lock_guard<std::mutex> lock(scheduledMutex);

                // Se o ator nao estiver na lista de agendamentos, nos o agendamos
                if (scheduledActors.find(actorID) == scheduledActors.end()) {
                    scheduledActors.insert(actorID);

                    // Abre uma thread separada apenas para a contagem do tempo (nao trava o jogo)
                    std::thread([actorID]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(400));

                        // Apos 200ms, agenda a tarefa na thread principal usando a interface do SKSE
                        SKSE::GetTaskInterface()->AddTask([actorID]() {

                            // Libera o ator da lista de agendamento para permitir execucoes futuras
                            {
                                std::lock_guard<std::mutex> lock(scheduledMutex);
                                scheduledActors.erase(actorID);
                            }

                            // Busca o ator com seguranca pelo ID.
                            // O ponteiro original poderia ser invalido apos 200ms.
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



