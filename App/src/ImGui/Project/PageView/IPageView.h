#pragma once

#include "Project/Project.h"

#include <string>

namespace LM
{

    class IPageView
    {
    public:
        IPageView();
        virtual ~IPageView() { }

        virtual void Draw();

        virtual void DrawOtherWindows() { }

        virtual bool OnPageWillBeChanged(int _CurrentPageId, int _NewPageId) { return true; }

        // virtual bool OnAppClose();
        virtual void Save() {};

    protected:
        void SetContext(Ref<Project> _Project, int _PageId);
        void ClearContext();

        virtual std::string GetBasePath() const = 0;
        virtual std::string GetFileName() const = 0;
        virtual const char* GetWindowName() const = 0;

        virtual void DrawWindowContent() = 0;

        virtual void DrawTopMenuExtras() {};
        virtual void DrawExtras() {};

    protected:
        friend class PageViewManager;

        Ref<Project> m_Project;
        int m_PageId = 0;
        // TODO: as std::filesystem::path
        std::string m_BasePath;

        bool m_IsDisableNextFrameInputEvents = false;

        const float kLineThickness = 2.0f;
    };

}    // namespace LM
