#include "Manager.h"

void EquipBestInventoryItems(RE::Actor* a_actor);

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
    // Nota: Como a função original é 'const', o ponteiro 'this' também deve ser const
    static bool Hook_ShouldBackgroundClone(const RE::TESObjectREFR* a_this) {
        auto actor = const_cast<RE::Actor*>(a_this->As<RE::Actor>());
        if (actor && !actor->IsDead() && actor != RE::PlayerCharacter::GetSingleton()) {
            auto settings = NPCSettings::GetSingleton();
            if (settings->autoEquip) {
                EquipBestInventoryItems(actor);
            }
        }

        return _ShouldBackgroundClone(a_this);
    }

    static inline REL::Relocation<decltype(&Hook_ShouldBackgroundClone)> _ShouldBackgroundClone;
};



