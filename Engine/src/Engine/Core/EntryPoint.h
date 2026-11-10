#pragma once

#include "Engine/Core/Application.h"
#include "Engine/Core/Base.h"

extern LM::Application* LM::CreateApplication(ApplicationCommandLineArgs args);

int main(int argc, char** argv)
{
#ifdef _WIN32
    std::setlocale(LC_CTYPE, ".UTF8");
#else
    std::setlocale(LC_CTYPE, "");    // ".UTF8" is a Windows-style locale name, no-op elsewhere
#endif
    std::cout << "Entry point start" << std::endl;

    auto App = LM::CreateApplication({ argc, argv });

    App->Run();

    delete App;

    return 0;
}
