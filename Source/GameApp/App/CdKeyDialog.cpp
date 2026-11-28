#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// CD Key Dialog
//
//////////////////////////////////////////////////////////////////////////////


class CDKeyDialogImpl : public IPopup, 
    public EventTargetContainer<CDKeyDialogImpl>
{
private:
    TRef<Pane> m_ppane;
    TRef<ButtonPane> m_pbuttonOK;
    TRef<ButtonPane> m_pbuttonCancel;
    TRef<EditPane> m_peditPane;

    TRef<IKeyboardInput> m_pkeyboardInputOldFocus;

public:
    
    CDKeyDialogImpl()
    {

        TRef<INameSpace> pns = GetModeler()->GetNameSpace("cdkeydialog");

        CastTo(m_ppane,                 pns->FindMember("CDKeyDialog"));

        CastTo(m_pbuttonOK,             pns->FindMember("CDKeyOkButtonPane"));
        CastTo(m_pbuttonCancel,         pns->FindMember("CDKeyCancelButtonPane"));
        CastTo(m_peditPane,      (Pane*)pns->FindMember("CDKeyEditPane"));

        m_peditPane->SetMaxLength(c_cbCDKey - 1);
        AddEventTarget(&CDKeyDialogImpl::OnButtonOK, m_pbuttonOK->GetEventSource());
        AddEventTarget(&CDKeyDialogImpl::OnButtonCancel, m_pbuttonCancel->GetEventSource());
    }

    //
    // IPopup methods
    //

    Pane* GetPane() override
    {
        return m_ppane;
    }

    ZString InsertHyphen(ZString strSource, int nOffset)
    {
        if ((int)strSource.size() < nOffset)
            return strSource;
        else
            return StrLeft(strSource, nOffset) + ZString("-") + StrRightOf(strSource, nOffset);
    }

    ZString FormatKey(const ZString& strSource)
    {
        ZString strResult = StrToUpper(strSource); 

        // remove all spaces
        StrRemoveAll(strResult, ' ');

        return strResult;
    }

    void OnClose() override
    {
        if (m_pkeyboardInputOldFocus)
            GetWindow()->SetFocus(m_pkeyboardInputOldFocus);

        m_pkeyboardInputOldFocus = nullptr;

        IPopup::OnClose();
    }

    void SetContainer(IPopupContainer* pcontainer) override
    {
        m_pkeyboardInputOldFocus = GetWindow()->GetFocus();
        GetWindow()->SetFocus(m_peditPane);
        m_peditPane->SetString(FormatKey(trekClient.GetCDKey()));

        IPopup::SetContainer(pcontainer);
    }

    bool OnKey(IInputProvider* pprovider, const KeyState& ks, bool& fForceTranslate) override
    {
        // we need to make sure we get OnChar calls to pass on to the edit box
        fForceTranslate = true;
        return false;
    }

    bool OnChar(IInputProvider* pprovider, const KeyState& ks) override
    {
        if (ks.vk == 13)
        {
            OnButtonOK();
            return true;
        }
        else
        {
            bool bResult = ((IKeyboardInput*)m_peditPane)->OnChar(pprovider, ks);

            ZString strResult = m_peditPane->GetString();
            
            // add or remove hyphens as appropriate
            if (ks.vk != 8) // let backspace still delete things, however
                strResult = FormatKey(strResult);

            m_peditPane->SetString(strResult);

            return bResult;
        }
    }

    bool OnButtonOK()
    {
        ZString strCDKey = m_peditPane->GetString();

        if (!CDKeyIsValid(strCDKey))
        {
            TRef<IMessageBox> pmsgBox = CreateMessageBox("\"" + strCDKey 
                + "\" is not a valid CD Key.  Please enter the CD Key from "
                "the back of your CD Case");
            GetWindow()->GetPopupContainer()->OpenPopup(pmsgBox, false);
        }
        else
        {
            GetWindow()->SetFocus(m_pkeyboardInputOldFocus);
            m_pkeyboardInputOldFocus = nullptr;

            trekClient.SetCDKey(strCDKey);

            if (m_ppopupOwner) {
                m_ppopupOwner->ClosePopup(this);
            } else {
                m_pcontainer->ClosePopup(this);
            }
        }

        return true;
    }

    bool OnButtonCancel()
    {
        GetWindow()->SetFocus(m_pkeyboardInputOldFocus);
        m_pkeyboardInputOldFocus = nullptr;

        if (m_ppopupOwner) {
            m_ppopupOwner->ClosePopup(this);
        } else {
            m_pcontainer->ClosePopup(this);
        }

        return true;
    }

    static bool CDKeyIsValid(const ZString& strCDKey)
    {
        // TODO: do a more sound check.
        return (int)strCDKey.size() > 0;
    }
};


TRef<IPopup> CreateCDKeyPopup()
{
    return new CDKeyDialogImpl();
}
