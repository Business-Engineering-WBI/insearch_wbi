#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace LM
{

    class TextureLoader
    {
    public:
        TextureLoader(std::string_view _FileName);
        ~TextureLoader();

        inline uint32_t GetWidth() const { return m_Width; }
        inline uint32_t GetHeight() const { return m_Height; }
        inline uint32_t GetChannels() const { return m_Channels; }
        inline const uint8_t* const GetData() const { return m_Data; }

        bool IsOk() const { return m_Data; }

    protected:
        int m_Width;
        int m_Height;
        int m_Channels;
        uint8_t* m_Data;
    };

}    // namespace LM
