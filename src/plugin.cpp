#include "logger.h"
#include "Settings.h"
#include "Hooks.h"

namespace
{
    bool hasDFG = false;

    void ConvertConfiguredOutfits()
    {
        auto settings = NPCSettings::GetSingleton();

        // So executa a conversao se o modo nao for 'Disabled'
        if (settings && settings->outfitMode != OutfitConversionMode::kDisabled) {
            Manager::GetSingleton()->ConvertAllNPCOutfitsToInventory();
        }
    }

    void PopulateListsAndConvertOutfits()
    {
        Manager::GetSingleton()->PopulateAllLists();
        ConvertConfiguredOutfits();
    }

    void RefreshListsAndConvertOutfits()
    {
        auto manager = Manager::GetSingleton();
        manager->_isPopulated = false;
        manager->PopulateAllLists();
        ConvertConfiguredOutfits();
    }

    class DynamicFormsGeneratorListener : public RE::BSTEventSink<SKSE::ModCallbackEvent>
    {
    public:
        static DynamicFormsGeneratorListener* GetSingleton()
        {
            static DynamicFormsGeneratorListener singleton;
            return std::addressof(singleton);
        }

        void Register()
        {
            if (auto source = SKSE::GetModCallbackEventSource()) {
                source->AddEventSink(this);
            }
        }

        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
        {
            if (!a_event) {
                return RE::BSEventNotifyControl::kContinue;
            }

            const std::string_view eventName = a_event->eventName.c_str();
            if (eventName == "DynamicFormsGeneratorLoaded") {
                PopulateListsAndConvertOutfits();
                return RE::BSEventNotifyControl::kContinue;
            }

            if (eventName == "DynamicFormsGeneratorUpdated") {
                RefreshListsAndConvertOutfits();
                return RE::BSEventNotifyControl::kContinue;
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kPostLoad) {
        hasDFG = GetModuleHandleA("DynamicFormsGenerator.dll") != nullptr;
        if (hasDFG) {
            logger::info("DynamicFormsGenerator.dll found");
        }
        RDO_UI::Register();
    }

    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        if (!hasDFG) {
            PopulateListsAndConvertOutfits();
        }
        //MyPackageEventHandler::Register();
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
    DynamicFormsGeneratorListener::GetSingleton()->Register();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
