#ifndef _logon_h_
#define _logon_h_

//////////////////////////////////////////////////////////////////////////////
//
// logon
//
//////////////////////////////////////////////////////////////////////////////

class LogonSite : public IObject {
public:
    virtual void OnLogon(const ZString& strName) = 0;
    virtual void OnAbort() = 0;
};

// Asks for a call sign
TRef<IPopup> CreateLogonPopup(Modeler* pmodeler, LogonSite* psite, LPCSTR szPrompt, LPCSTR szName);

#endif
