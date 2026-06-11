#include "IImgPageView.h"

namespace LM
{

    class RawImgPageView : public IImgPageView
    {
    protected:
        enum class PatternState
        {
            kNone = 0,

            kCreateTopLeft,
            kCreateBotRight,

            kShowTopLeft,
            kShowBotRight
        };

        enum class PatternPointState
        {
            kNone = 0,

            kFirst,
            kSecond,
            kCenter
        };

    protected:
        virtual std::string GetBasePath() const override { return m_Project->GetPdfTablesWithOcrTypeRawImgPrevPath(); }
        virtual const char* GetWindowName() const override { return "Начальные картинки"; }

        virtual void DrawTopMenuExtras() override;
        virtual void DrawExtras() override;
        virtual void DrawCreateCutPattern();
        virtual void DrawShowCutPattern();

        virtual void DrawCutPattern();

        bool IsCutPatternCreateState() const;
        bool IsCutPatternShowState() const;

        void SavePattern();
        void CancelPattern();

        void DrawPatternPointButton(PatternPointState _PointState, int id);

    protected:
        PatternState m_PatternState = PatternState::kNone;
        PatternPointState m_PatternPointState = PatternPointState::kNone;
        CatalogCutPattern m_TmpCatalogCutPattern;
    };

}    // namespace LM
