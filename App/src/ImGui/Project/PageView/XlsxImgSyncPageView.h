#include "IPageView.h"

namespace LM
{

    class XlsxImgSyncPageView : public IPageView
    {

    protected:
        std::string GetFileName() const override;
        virtual std::string GetBasePath() const override { return m_Project->GetPdfTablesWithOcrTypeRawImgPrevPath(); }
        virtual const char* GetWindowName() const override { return "Синхронный каталог"; }

        void DrawWindowContent() override;
        virtual void DrawTopMenuExtras() override;
        virtual void DrawExtras() override;

        std::optional<std::filesystem::path> GetFileByIndex(const std::filesystem::path& _Folder, std::size_t _Index);

    protected:
        int m_PageOffset = 0;
        float m_Scale = 1.0f;

        int m_OpenedPageId = -1;
        std::string m_OpenedFileName;
    };

}    // namespace LM
