#include "PythonCommand.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

#ifndef _WIN32
    #include <sys/wait.h>    // WIFEXITED / WEXITSTATUS
#endif

#include "Engine/Utils/Log.hpp"

namespace LM
{

#ifdef _WIN32
    #define pclose _pclose
    #define popen  _popen
#endif

    PythonCommand::PythonCommand(std::string_view _Script) : m_Script(_Script) { }

    void PythonCommand::AddArg(std::string_view _Arg, std::string_view _ArgName)
    {
        m_Args.emplace_back(ArgType { .Arg = _Arg.data(), .ArgName = _ArgName.data() });
    }

    void PythonCommand::AddPathArg(const std::filesystem::path& _Arg, std::string_view _ArgName)
    {
        m_Args.emplace_back(ArgType { .Arg = _Arg.string().c_str(), .ArgName = _ArgName.data() });
    }

    void PythonCommand::AddArg(bool _Arg, std::string_view _ArgName)
    {
        m_Args.emplace_back(ArgType { .Arg = _Arg ? "1" : "0", .ArgName = _ArgName.data() });
    }

    void PythonCommand::AddArg(float _Arg, std::string_view _ArgName)
    {
        m_Args.emplace_back(ArgType { .Arg = std::to_string(_Arg), .ArgName = _ArgName.data() });
    }

    void PythonCommand::AddArg(int _Arg, std::string_view _ArgName)
    {
        m_Args.emplace_back(ArgType { .Arg = std::to_string(_Arg), .ArgName = _ArgName.data() });
    }

    std::string PythonCommand::GetFullCommand() { return GetCommand() + ' ' + m_Script + ' ' + GetArgs(); }

    std::string PythonCommand::GetCommand()
    {
#ifdef _WIN32
        return ".\\.venv\\scripts\\python";
#else
        return ".venv/scripts/python";
#endif
    }

    std::string PythonCommand::GetArgs()
    {
        std::string result;
        for (const auto& arg : m_Args)
        {
            result += arg.ArgName + ' ' + '"' + arg.Arg + '"' + ' ';
        }
        return result;
    }

    int PythonCommand::Execute(std::function<void(const char*)> _LinePrintCallback)
    {
        std::array<char, 1024> buffer;

        std::string command = GetFullCommand() + " 2>&1";    // Перенаправляем stderr в stdout
        LOG_CORE_INFO("Start python command: {}", command);

        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe)
        {
            throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
        {
            std::cout << "[Python]: " << buffer.data();
            if (_LinePrintCallback)
            {
                _LinePrintCallback(buffer.data());
            }
        }

        int exitCode = pclose(pipe.release());

#ifdef _WIN32
        // В Windows pclose возвращает код возврата напрямую
        LOG_CORE_INFO("Python command finished with exit code: {}", exitCode);
#else
        // В Unix-системах нужно использовать WEXITSTATUS
        if (WIFEXITED(exitCode))
        {
            exitCode = WEXITSTATUS(exitCode);
            LOG_CORE_INFO("Python command finished with exit code: {}", exitCode);
        }
        else
        {
            LOG_CORE_ERROR("Python command terminated abnormally");
            exitCode = -1;
        }
#endif

        if (exitCode != 0)
        {
            LOG_CORE_ERROR("Python command failed with exit code: {}", exitCode);
        }

        return exitCode;
    }

}    // namespace LM
