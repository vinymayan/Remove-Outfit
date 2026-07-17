#pragma once
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "Manager.h"

namespace RDO_UI {
    // Registra a UI no SKSEMenuFramework
    void Register();

    // Renderiza a aba de configuracoes
    void RenderSettings();

    const char* GetLoc(const std::string& key, const char* fallback);
    void LoadLanguage();
}
