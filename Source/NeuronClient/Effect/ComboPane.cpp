#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

class StringComboFacePane : public ComboFacePane {
private:
    TRef<StringPane> m_pstringPane;
    TRef<BorderPane> m_pborder;

public:
    StringComboFacePane(
        const WinPoint&    size,
              IEngineFont* pfont,
        const Color&       color,
              bool         bBackgroundColor
    ) {
        InternalSetSize(size);

        m_pstringPane =
            new StringPane(
                ZString(),
                pfont,
                size
            );

        m_pstringPane->SetTextColor(color);

        if (bBackgroundColor) {
            InsertAtBottom(
                m_pborder = new BorderPane(
                    0,
                    Color(0.25, 0.25, 0.25),
                    m_pstringPane
                )
            );
        } else {
            InsertAtBottom(m_pstringPane);
        }
    }

    void SetString(const ZString& str) override
    {
        m_pstringPane->SetString(str);
    }

    void SetInside(bool bInside) override
    {
    }

    void SetColor(const Color& color) override
    {
        if (m_pborder) {
            m_pborder->SetColor(color);
        }
    }
};

TRef<ComboFacePane> CreateStringComboFacePane(
    const WinPoint&    size,
          IEngineFont* pfont,
    const Color&       color,
          bool         bBackgroundColor
) {
    return new StringComboFacePane(size, pfont, color, bBackgroundColor);
}

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

class ImageComboFacePane : public ComboFacePane {
private:
    TRef<ButtonFacePane> m_pfacePane;

public:
    ImageComboFacePane(Image* pimage)
    {
        m_pfacePane =
            CreateButtonFacePane(
                pimage->GetSurface(),
                ButtonFaceUp | ButtonFaceInside
            );

        InsertAtBottom(m_pfacePane);
    }

    void SetString(const ZString& str) override
    {
    }

    void SetColor(const Color& color) override
    {
    }

    void SetInside(bool bInside) override
    {
        m_pfacePane->SetInside(bInside);
    }

    void UpdateLayout() override
    {
        DefaultUpdateLayout();
    }
};

TRef<ComboFacePane> CreateImageComboFacePane(Image* pimage)
{
    return new ImageComboFacePane(pimage);
}

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

class ComboPaneImpl :
    public ComboPane,
    public IMenuCommandSink
{
private:
    TRef<IntegerEventSourceImpl> m_peventSource;
    TRef<EventSourceImpl>        m_peventMouseEnterWhileEnabledSource;
    TRef<EventSourceImpl>        m_peventMenuSelectSource;
    TRef<IMenu>                  m_pmenu;
    TRef<Modeler>                m_pmodeler;
    TRef<IEngineFont>            m_pfont;
    TRef<ComboFacePane>          m_pface;
    TRef<IPopupContainer>        m_ppopupContainer;
    bool                         m_bFirstItem;
    std::vector<ZString>             m_vstrItems;
    int                          m_idSelection;
    bool                         m_bEnabled;
    bool                         m_bInside;

public:
    ComboPaneImpl(
        Modeler*         pmodeler,
        IPopupContainer* ppopupContainer,
        IEngineFont*     pfont,
        const WinPoint&  size,
        ComboFacePane*   pface
    ) :
        m_peventMouseEnterWhileEnabledSource(new EventSourceImpl()),
        m_peventMenuSelectSource(new EventSourceImpl()),
        m_ppopupContainer(ppopupContainer),
        m_pface(pface),
        m_bFirstItem(true),
        m_idSelection(0),
        m_bInside(false),
        m_bEnabled(true),
        m_pmodeler(pmodeler),
        m_pfont(pfont)
    {
        InsertAtBottom(
            new JustifyPane(
                JustifyPane::Left,
                size,
                pface
            )
        );
        m_peventSource     = new IntegerEventSourceImpl();
        m_pmenu =
            CreateMenu(
                pmodeler,
                pfont,
                IMenuCommandSink::CreateDelegate(this)
            );
    }

    ~ComboPaneImpl()
    {
        m_pmenu->SetMenuCommandSink(NULL);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // IEventSink
    //
    //////////////////////////////////////////////////////////////////////////////

    bool OnEvent(IEventSource* peventSource)
    {
        return true;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // IMenuCommandSink
    //
    //////////////////////////////////////////////////////////////////////////////

    void OnMenuCommand(IMenuItem* pitem) override
    {
        m_pface->SetString(pitem->GetString());
        m_pface->SetColor(pitem->GetColor());
        m_idSelection = pitem->GetID();
        m_peventMenuSelectSource->Trigger();
        m_peventSource->Trigger(pitem->GetID());
        m_ppopupContainer->ClosePopup(m_pmenu);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // ComboPane methods
    //
    //////////////////////////////////////////////////////////////////////////////

    void ClearContents() override
    {
        m_bFirstItem = true;
        m_idSelection = 0;
        m_vstrItems.clear();

        m_pmenu =
            CreateMenu(
                m_pmodeler,
                m_pfont,
                IMenuCommandSink::CreateDelegate(this)
            );
    }
    
    TRef<IMenuItem> AddItem(const ZString& str, int id) override
    {
        TRef<IMenuItem> pitem = m_pmenu->AddMenuItem(id, str);
        if (m_bFirstItem) {
            m_bFirstItem = false;
            m_idSelection = id;
            m_pface->SetString(pitem->GetString());
            m_pface->SetColor(pitem->GetColor());
        }
        m_vstrItems.push_back(str);

        return pitem;
    }

    TRef<IMenuItem> AddItem(const ZString& str, int id, const Color& color) override
    {
        bool bFirstItem = m_bFirstItem;

        TRef<IMenuItem> pitem = m_pmenu->AddMenuItem(id, str);

        if (bFirstItem) {
            m_pface->SetColor(color);
        }

        return pitem;
    }

    void SetSelection(int id) override
    {
        m_idSelection = id;
        IMenuItem* pitem = m_pmenu->FindMenuItem(id);

        if (pitem) {
            m_pface->SetString(pitem->GetString());
            m_pface->SetColor(pitem->GetColor());
            m_peventSource->Trigger(id);
        } else {
            m_pface->SetString("");
            m_peventSource->Trigger(id);
        }
    }

    const ZString * GetSelectionString() override
    {
        return &m_vstrItems[m_idSelection];
    }

    int GetSelection() override
    {
        return m_idSelection;
    }

    IIntegerEventSource* GetEventSource() override
    {
        return m_peventSource;
    }

    void SetEnabled(bool bEnabled) override
    {
        if (m_bEnabled != bEnabled)
        {
            m_bEnabled = bEnabled;

            if (bEnabled && m_bInside)
                m_peventMouseEnterWhileEnabledSource->Trigger();
        }
    }

    IEventSource* GetMouseEnterWhileEnabledEventSource() override
    {
        return m_peventMouseEnterWhileEnabledSource;
    }

    IEventSource* GetMenuSelectEventSource() override
    {
        return m_peventMenuSelectSource;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Pane methods
    //
    //////////////////////////////////////////////////////////////////////////////

    void UpdateLayout() override
    {
        DefaultUpdateLayout();
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // IMouseInput methods
    //
    //////////////////////////////////////////////////////////////////////////////

    void MouseEnter(IInputProvider* pprovider, const Point& point) override
    {
        m_bInside = true;
        m_pface->SetInside(true);

        if (m_bEnabled)
            m_peventMouseEnterWhileEnabledSource->Trigger();
    }

    void MouseLeave(IInputProvider* pprovider) override
    {
        m_bInside = false;
        m_pface->SetInside(false);
    }

    MouseResult HitTest(IInputProvider* pprovider, const Point& point, bool bCaptured) override
    {
        bool bInside =
               !IsHidden()
            && point.X() >= 0
            && point.X() < (float)XSize()
            && point.Y() >= 0
            && point.Y() < (float)YSize();

        return bInside ? MouseResultHit() : MouseResult();
    }

    MouseResult Button(IInputProvider* pprovider, const Point& point, int button, bool bCaptured, bool bInside, bool bDown) override
    {
        if (button == 0 && m_bEnabled) {
            if (bDown) {
                Point point =
                    TransformLocalToImage(
                        WinPoint(
                            0,
                            m_pface->GetSize().Y()
                        )
                    );

                m_ppopupContainer->OpenPopup(m_pmenu, Rect(point,point), true, true);
            }
        }

        return MouseResult();
    }
};

TRef<ComboPane> CreateComboPane(
    Modeler*         pmodeler,
    IPopupContainer* ppopupContainer,
    IEngineFont*     pfont,
    const WinPoint&  size,
    ComboFacePane*   pface
) {
    return new ComboPaneImpl(pmodeler, ppopupContainer, pfont, size, pface);
}
