#include "logger.h"
#include "Settings.h"
#include "Hooks.h"
void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {

        Manager::GetSingleton()->PopulateAllLists();
        auto settings = NPCSettings::GetSingleton();

        // Só executa a conversão se o modo não for 'Disabled'
        if (settings && settings->outfitMode != OutfitConversionMode::kDisabled) {
            Manager::GetSingleton()->ConvertAllNPCOutfitsToInventory();
        }
        //MyPackageEventHandler::Register();
        RDO_UI::Register();
    }
    if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        // Post-load
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    NPCSettings::GetSingleton()->Load();
    BackgroundCloneHook::Install();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
