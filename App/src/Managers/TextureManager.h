#pragma once

#include "Engine/Textures/Texture2D.h"

#include <unordered_map>

namespace LM
{

    class TextureManager
    {
    public:
        static Ref<Texture2D> AddOrReplace(std::string_view _FileName,
                                           Texture2D::MASK _Mask = Texture2D::MASK::NO_SRGB);
        static void RemoveFile(std::string_view _FileName);
        static void RemoveFolder(std::string_view _Folder);
        static Ref<Texture2D> Get(std::string_view _FileName);
        static bool Contains(std::string_view _FileName);

        static void RemoveAll();

    protected:
        static inline std::unordered_map<std::string, std::unordered_map<std::string, Ref<Texture2D>>> m_Bank;
    };

}    // namespace LM
