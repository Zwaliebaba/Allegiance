#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// Logon Popup
//
//////////////////////////////////////////////////////////////////////////////

class LogonPopup :
    public IPopup,
    public EventTargetContainer<LogonPopup>,
    public TEvent<ZString>::Sink
{
private:
    TRef<Pane>           m_ppane;
    TRef<ButtonPane>     m_pbuttonLogon;
    TRef<ButtonPane>     m_pbuttonAbort;
    TRef<EditPane>       m_peditName;
    TRef<LogonSite>      m_psite;
    TRef<IKeyboardInput> m_pfocus;

public:
    LogonPopup(Modeler* pmodeler, LogonSite* psite, LPCSTR szPrompt, LPCSTR szName) :
        m_psite(psite)
    {
        TRef<INameSpace> pnsLogonData = pmodeler->CreateNameSpace("logondata", pmodeler->GetNameSpace("gamepanes"));
        pnsLogonData->AddMember("promptText", new ModifiableString(szPrompt));

        TRef<INameSpace> pns = pmodeler->GetNameSpace("logon");
        CastTo(m_ppane, pns->FindMember("popup"));

        CastTo(m_pbuttonLogon,        pns->FindMember("logonButtonPane"));
        AddEventTarget(&LogonPopup::OnButtonLogon, m_pbuttonLogon->GetEventSource());
        CastTo(m_pbuttonAbort,        pns->FindMember("abortButtonPane"));
        AddEventTarget(&LogonPopup::OnButtonAbort, m_pbuttonAbort->GetEventSource());
        CastTo(m_peditName,     (Pane*)pns->FindMember("nameEditPane"    ));
        AddEventTarget(&LogonPopup::OnNameClick,     m_peditName->GetClickEvent());

        m_peditName->SetString(szName);
        m_peditName->SetMaxLength(c_cbName - 1);

        SetFocus(m_peditName);

        pmodeler->UnloadNameSpace(pns);
    }

    bool OnButtonLogon()
    {
        // Make sure that they have entered a CD Key
        if (g_bAskForCDKey && trekClient.GetCDKey().empty())
        {
            TRef<IPopup> ppopupCDKey = CreateCDKeyPopup();
            GetWindow()->GetPopupContainer()->OpenPopup(ppopupCDKey, false);
        }
        else
        {
            //
            // Grab info from the controls
            //
            ZString strName = m_peditName->GetString();

            if (strName[0] != ' ' && strName[0] != '\0') // give 'em a msg box?
            {
                //
                // Close
                //

                TRef<LogonPopup> pthis = this;
                ClosePopup(nullptr);


                //
                // Tell the site
                //

                m_psite->OnLogon(strName);
            }
        }

        return true;
    }

    bool OnButtonAbort()
    {
        m_psite->OnAbort();
        ClosePopup(nullptr);
        return true;
    }

    void SetFocus(IKeyboardInput* pfocus) 
    {
        if (m_pfocus) {
            m_pfocus->SetFocusState(false);
        }
        m_pfocus = pfocus;
        m_pfocus->SetFocusState(true);
    }

    bool OnEvent(TEvent<ZString>::Source* psource, ZString str) override
    {
        m_pbuttonLogon->SetEnabled(!str.empty());
        return true;
    }

    bool OnNameClick()
    {
        SetFocus(m_peditName);
        return true;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // IPopup Methods
    //
    //////////////////////////////////////////////////////////////////////////////

    void SetContainer(IPopupContainer* pcontainer) override
    {
        IPopup::SetContainer(pcontainer);

        if (g_bQuickstart)
        {
            OnButtonLogon();
        }
    }

    Pane* GetPane() override
    {
        return m_ppane;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // IKeyboardInput Methods
    //
    //////////////////////////////////////////////////////////////////////////////

    bool OnKey(IInputProvider* pprovider, const KeyState& ks, bool& fForceTranslate) override
    {
        if (ks.bDown) 
        {
            if (ks.vk == VK_RETURN) 
            {
                if (m_pbuttonLogon->GetEnabled()) 
                {
                    OnButtonLogon();
                } 
                else 
                {
                    OnButtonAbort();
                }
                return true;
            } 
            else if (ks.vk == VK_ESCAPE) 
            {
                OnButtonAbort();
                return true;
            } 
        }

        return m_pfocus->OnKey(pprovider, ks, fForceTranslate);
    }

    bool OnChar(IInputProvider* pprovider, const KeyState& ks) override
    {
        return m_pfocus->OnChar(pprovider, ks);
    }
};

//////////////////////////////////////////////////////////////////////////////
//
// Constructor
//
//////////////////////////////////////////////////////////////////////////////

TRef<IPopup> CreateLogonPopup(Modeler* pmodeler, LogonSite* psite, LPCSTR szPrompt, LPCSTR szName)
{
    return new LogonPopup(pmodeler, psite, szPrompt, szName);
}


