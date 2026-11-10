#pragma once

#include <stdexcept>
#include <tuple>

#include "Engine/Core/Base.h"
#include "Engine/Utils/Log.hpp"
#include "Engine/Utils/json.hpp"

#include "Project/Project.h"
#include "Project/ProjectVariantExcelTables.hpp"

namespace LM
{

    namespace Concept
    {

        template <typename T>
        concept Integral = std::is_integral<T>::value;

        template <typename T>
        concept FloatingPoint = std::is_floating_point<T>::value;

        template <typename T>
        concept StringLike = std::is_convertible_v<T, std::string_view>;

        template <typename T>
        concept JsonSimpleType = Integral<T> || FloatingPoint<T> || StringLike<T>;

        template <typename T>
        concept Vec2 = std::is_same_v<T, glm::vec2>;

        template <typename T>
        concept Array = std::is_array_v<T>;

        template <typename T>
        concept ProjectTypeConcept = std::is_same_v<T, ProjectVariant>;

        template <typename T>
        concept GenRawExcelConcept = std::is_same_v<T, GenRawExcel>;

        // TODO: create concepts for this type: GenImgsByCutPattern Project Catalog CatalogCutPattern
        template <typename T>
        concept GenImgsByCutPatternConcept = std::is_same_v<T, GenImgsByCutPattern>;

        template <typename T>
        concept ProjectConcept = std::is_same_v<T, Project>;

        template <typename T>
        concept CatalogConcept = std::is_same_v<T, Catalog>;

        template <typename T>
        concept CatalogCutPatternConcept = std::is_same_v<T, CatalogCutPattern>;

    }    // namespace Concept

    template <typename T>
    struct PropertyImplExtraProps
    {
        bool IsRequired = true;
        bool IsFillWithDefaultIfNotFound = false;
        void* (*GetDefaultValue)() = nullptr;
    };

    template <typename Class, typename T>
    struct PropertyImpl
    {
        constexpr PropertyImpl(T Class::*_Member, const char* _Name, const PropertyImplExtraProps<T>& _ExtraProps = {})
            : Member { _Member },
              Name { _Name },
              ExtraProps { _ExtraProps }
        { }

        using Type = T;

        T Class::*Member;
        const char* Name;
        PropertyImplExtraProps<T> ExtraProps;
    };

    template <typename Class, typename T>
    constexpr auto property(T Class::*_Member, const char* _Name, const PropertyImplExtraProps<T>& _ExtraProps = {})
    {
        return PropertyImpl<Class, T> { _Member, _Name, _ExtraProps };
    }

#define PROPERTY(CLASS, MEMBER)         property(&CLASS::MEMBER, #MEMBER)
#define PROPERTY_NOT_REQ(CLASS, MEMBER) property(&CLASS::MEMBER, #MEMBER, false)

    template <typename T, T... S, typename F>
    constexpr void for_sequence(std::integer_sequence<T, S...>, F&& _Func)
    {
        (static_cast<void>(_Func(std::integral_constant<T, S> {})), ...);
    }

    // ============================================================================================================
    // ============ GetProperties =================================================================================
    // ============================================================================================================

    struct SerializerGetPropertiesAll
    {
        template <typename Class>
        static constexpr auto GetProperties()
        {
            static_assert("Properties not specified!");
            return std::make_tuple<int>(1);
        }

    };

    // Явные специализации статического шаблонного члена допустимы только в области
    // видимости пространства имён (MSVC разрешал их внутри класса как расширение,
    // gcc отклоняет: "explicit specialization in non-namespace scope").
    template <>
    constexpr auto SerializerGetPropertiesAll::GetProperties<GenRawExcel>()
    {
        return std::make_tuple(                                        //
            property(&GenRawExcel::UseCutPattern, "UseCutPattern"),    //
            property(&GenRawExcel::IsGenerated, "IsGenerated"),        //
            property(&GenRawExcel::NeedRebuild, "NeedRebuild"),        //
            property(&GenRawExcel::Version, "Version"));
    }

    template <>
    constexpr auto SerializerGetPropertiesAll::GetProperties<GenImgsByCutPattern>()
    {
        return std::make_tuple(                                            //
            property(&GenImgsByCutPattern::IsGenerated, "IsGenerated"),    //
            property(&GenImgsByCutPattern::NeedRebuild, "NeedRebuild"),    //
            property(&GenImgsByCutPattern::Version, "Version"));
    }

    template <>
    constexpr auto SerializerGetPropertiesAll::GetProperties<Project>()
    {
        return std::make_tuple(                               //
            property(&Project::m_Type, "Type", { false }),    //
            property(&Project::m_Folder, "Folder"),           //
            property(&Project::m_Catalog, "Catalog"),         //
            property(&Project::m_VariantExcelTables, "VariantExcelTables", { false }));
    }

    template <>
    constexpr auto SerializerGetPropertiesAll::GetProperties<Catalog>()
    {
        return std::make_tuple(                                              //
            property(&Catalog::BaseFileName, "BaseFileName"),                //
            property(&Catalog::ImgQuality, "ImgQuality"),                    //
            property(&Catalog::SplitPages, "SplitPages"),                    //
            property(&Catalog::NeedRebuild, "NeedRebuild"),                  //
            property(&Catalog::IsGenerated, "IsGenerated"),                  //
            property(&Catalog::BotRightCutPattern, "BotRightCutPattern"),    //
            property(&Catalog::TopLeftCutPattern, "TopLeftCutPattern"),
            property(&Catalog::GeneratedCatalogExcludePages, "GeneratedCatalogExcludePages", { false }),    //
            property(&Catalog::GenImgsByCutPattern, "GenImgsByCutPattern"),                                 //
            property(&Catalog::GenRawExcel, "GenRawExcel"));
    }

    template <>
    constexpr auto SerializerGetPropertiesAll::GetProperties<CatalogCutPattern>()
    {
        return std::make_tuple(                                    //
            property(&CatalogCutPattern::PageId, "PageId"),        //
            property(&CatalogCutPattern::PointMin, "PointMin"),    //
            property(&CatalogCutPattern::PointMax, "PointMax"),    //
            property(&CatalogCutPattern::CenterPoint, "CenterPoint"));
    }

    template <>
    constexpr auto SerializerGetPropertiesAll::GetProperties<ProjectVariantExcelTables>()
    {
        return std::make_tuple(    //
            property(&ProjectVariantExcelTables::m_PageNamesToSkipOnServerImport, "PageNamesToSkipOnServerImport",
                     { false }));
    }

    template <typename Derived>
    struct SerializerBase_0_0_0
    {
        // ============================================================================================================
        // ============ ToJson ========================================================================================
        // ============================================================================================================

        template <Concept::JsonSimpleType T>
        inline nlohmann::json ToJson(const T& _Object) const
        {
            return _Object;
        }

        template <Concept::Vec2 T>
        inline nlohmann::json ToJson(const T& _Object) const
        {
            nlohmann::json result;
            result["x"] = _Object.x;
            result["y"] = _Object.y;
            return result;
        }

        template <typename T>
        inline nlohmann::json ToJson(const std::vector<T>& _Object) const
        {
            nlohmann::json result = nlohmann::json::array();
            for (const auto& item : _Object)
            {
                result.push_back(ToJson(item));
            }
            return result;
        }

        template <Concept::ProjectTypeConcept T>
        inline nlohmann::json ToJson(const T& _Object) const
        {
            return static_cast<uint32_t>(_Object);
        }

        template <typename T>
        inline nlohmann::json ToJson(const T& _Object) const
        {
            nlohmann::json result;

            constexpr auto properties = SerializerGetPropertiesAll::GetProperties<T>();
            constexpr auto nbProperties = std::tuple_size<decltype(properties)>::value;

            for_sequence(std::make_index_sequence<nbProperties> {}, [&](auto i) {
                auto property = std::get<i>(properties);
                result[property.Name] = ToJson(_Object.*(property.Member));
            });

            return result;
        }

        // ============================================================================================================
        // ============ FromJson ======================================================================================
        // ============================================================================================================

        template <Concept::JsonSimpleType T>
        inline void FromJson(T& _Object, const nlohmann::json& _Json) const
        {
            _Object = _Json;
        }

        template <Concept::Vec2 T>
        inline void FromJson(T& _Object, const nlohmann::json& _Json) const
        {
            _Object = { _Json["x"], _Json["y"] };
        }

        template <typename T>
        inline void FromJson(std::vector<T>& _Object, const nlohmann::json& _Json) const
        {
            for (const auto& item : _Json)
            {
                _Object.push_back(item);
            }
        }

        template <Concept::ProjectTypeConcept T>
        inline void FromJson(T& _Object, const nlohmann::json& _Json) const
        {
            _Object = static_cast<ProjectVariant>(_Json);
        }

        template <typename T>
        inline void FromJson(T& _Object, const nlohmann::json& _Json) const
        {
            constexpr auto properties = SerializerGetPropertiesAll::GetProperties<T>();
            constexpr auto nbProperties = std::tuple_size<decltype(properties)>::value;

            for_sequence(std::make_index_sequence<nbProperties> {}, [&](auto i) {
                auto property = std::get<i>(properties);
                using Type = typename decltype(property)::Type;
                if (!_Json.contains(property.Name))
                {
                    if (property.ExtraProps.IsRequired)
                    {
                        throw std::invalid_argument(property.Name);
                    }

                    // TODO: Check for default value creation?
                    _Object.*(property.Member) = Type();
                    return;
                }

                FromJson(_Object.*(property.Member), _Json[property.Name]);
            });
        }
    };

    // ============================================================================================================
    // ============ Serializer Versions ===========================================================================
    // ============================================================================================================

    struct Serializer_0_0_0 : public SerializerBase_0_0_0<Serializer_0_0_0>
    {
    };

    template <typename Ser>
    struct TSerializer
    {
    public:
        nlohmann::json Serialize(Ref<Project> _Project) { return Ser().ToJson(*_Project); }

        bool DeSerialize(Ref<Project> _Project, nlohmann::json _Json)
        {
            try
            {
                Ser().FromJson(*_Project, _Json);
            }
            catch (const std::invalid_argument& exeption)
            {
                LOG_CORE_ERROR("Can't find field: {}", exeption.what());
                return false;
            }
            return true;
        }
    };

    using SerializerLast = TSerializer<Serializer_0_0_0>;

}    // namespace LM
