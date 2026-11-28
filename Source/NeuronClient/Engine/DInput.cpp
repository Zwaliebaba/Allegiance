#include "pch.h"
#include "dinput.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

//////////////////////////////////////////////////////////////////////////////
//
// DDWrapers
//
//////////////////////////////////////////////////////////////////////////////

//
// Zero fill for structs with a dwSize member (was in the DirectDraw
// wrapper header ddstruct.h; DirectInput is its only remaining user)
//

template<class Type>
class TZeroFillWithSize : public Type {
protected:
    TZeroFillWithSize()
    {
        Reinitialize();
    }

public:
    void Reinitialize()
    {
        memset(this, 0, sizeof(Type));
        this->dwSize = sizeof(Type);
    }
};

class DIDeviceCaps : public TZeroFillWithSize<DIDEVCAPS> {
public:
};

class DIDeviceInstance : public TZeroFillWithSize<DIDEVICEINSTANCE> {
public:
};

//////////////////////////////////////////////////////////////////////////////
//
// 
//
//////////////////////////////////////////////////////////////////////////////

const DIDATAFORMAT* g_pdfDIMouse;

//////////////////////////////////////////////////////////////////////////////
//
// 
//
//////////////////////////////////////////////////////////////////////////////

class DDInputObject : public IObject {
private:
    ZString m_strName;
    DWORD   m_dwType;
    GUID    m_guidType;

public:
    DDInputObject(const ZString& strName, DWORD dwType, const GUID& guidType) :
        m_strName(strName),
        m_dwType(dwType),
        m_guidType(guidType)
    {
    }

    const ZString& GetName() const
    {
        return m_strName;
    }

    DWORD GetDWType() const
    {
        return m_dwType;
    }

    const GUID& GetGUID() const
    {
        return m_guidType;
    }
};

class ValueDDInputObject : public DDInputObject {
private:
    TRef<ModifiableNumber> m_pnumber;

public:
    ValueDDInputObject(const ZString& strName, DWORD dwID, const GUID& guidType) :
        DDInputObject(strName, dwID, guidType),
        m_pnumber(new ModifiableNumber(0))
    {                        
    }

    ModifiableNumber* GetValue() const
    {
        return m_pnumber;
    }
};

class ButtonDDInputObject : public DDInputObject {
private:
    TRef<ModifiableBoolean> m_pbool;

public:
    ButtonDDInputObject(const ZString& strName, DWORD dwID, const GUID& guidType) :
        DDInputObject(strName, dwID, guidType),
        m_pbool(new ModifiableBoolean(false))
    {
    }

    ModifiableBoolean* GetValue() const
    {
        return m_pbool;
    }
};

//////////////////////////////////////////////////////////////////////////////
//
// 
//
//////////////////////////////////////////////////////////////////////////////

class MouseInputStreamImpl : public MouseInputStream {
private:
    //////////////////////////////////////////////////////////////////////////////
    //
    // 
    //
    //////////////////////////////////////////////////////////////////////////////
    
    BOOL EnumObjectsCallback(
        LPCDIDEVICEOBJECTINSTANCE pddoi
    ) {
        if (
               pddoi->dwType & DIDFT_AXIS
            || pddoi->dwType & DIDFT_POV
        ) {
            int index;

                   if (pddoi->guidType == GUID_XAxis) {
                index = 0;                           
            } else if (pddoi->guidType == GUID_YAxis) {
                index = 1;
            } else if (pddoi->guidType == GUID_ZAxis) {
                index = 2;
            } else {
                index = -1;
            }

            ValueDDInputObject* pobject =
                new ValueDDInputObject(
                    pddoi->tszName,
                    pddoi->dwType,
                    pddoi->guidType
                );

            if (index == -1) {
                m_vvalueObject.push_back(pobject);
            } else {
                m_vvalueObject[index] = pobject;
            }
        } else if (pddoi->dwType & DIDFT_PSHBUTTON) {
            ButtonDDInputObject* pobject =
                new ButtonDDInputObject(
                    pddoi->tszName,
                    pddoi->dwType,
                    pddoi->guidType
                );

            m_vbuttonObject.push_back(pobject);
        } 

        return DIENUM_CONTINUE;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // 
    //
    //////////////////////////////////////////////////////////////////////////////
    
    static BOOL CALLBACK StaticEnumObjectsCallback(
        LPCDIDEVICEOBJECTINSTANCE lpddoi,
        LPVOID                    pvRef
    ) {
        MouseInputStreamImpl* pthis = (MouseInputStreamImpl*)pvRef;
        return pthis->EnumObjectsCallback(lpddoi);
    }
 
    //////////////////////////////////////////////////////////////////////////////
    //
    // members
    //
    //////////////////////////////////////////////////////////////////////////////
    
    TRef<IDirectInputDevice8>           m_pdid;
    TRef<ButtonEvent::SourceImpl>       m_pbuttonEventSource;
    DIDeviceCaps                        m_didc;
    DIDeviceInstance                    m_didi;
    std::vector<TRef<ValueDDInputObject >>  m_vvalueObject;
    std::vector<TRef<ButtonDDInputObject>>  m_vbuttonObject;
    Rect                                m_rect;
    Point                               m_point;
    float                               m_z;
    bool                                m_bBuffered;
    bool                                m_bEnabled;
    int                                 m_threshold1;
    int                                 m_threshold2;
    int                                 m_acceleration;
    float                               m_sensitivity;

public:
    //////////////////////////////////////////////////////////////////////////////
    //
    // Constructor
    //
    //////////////////////////////////////////////////////////////////////////////
    
    MouseInputStreamImpl(IDirectInputDevice8* pdid, HWND hwnd) :
        m_pdid(pdid),
        m_rect(0, 0, 0, 0),                                                                         
        m_point(0, 0),
        m_vvalueObject(3),
        m_bEnabled(false),
        m_bBuffered(true),
        m_pbuttonEventSource(ButtonEvent::Source::Create())
    {
        //
        // Are we running on NT
        //

        /*!!!
        OSVERSIONINFO osvi = { sizeof(osvi) };
        ZVerify(GetVersionEx(&osvi));

        if ((VER_PLATFORM_WIN32_NT & osvi.dwPlatformId) != 0) {
            m_bBuffered = false;
        }
        */

        //
        // Enumerate the buttons and values
        //

        DDCall(m_pdid->EnumObjects(StaticEnumObjectsCallback, this, DIDFT_ALL));

        //
        // Setup the device
        //

        SetupDevice();

        //
        // We only need mouse input when we are in the foreground
        //

        DDCall(m_pdid->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND));

        //
        // Get the mouse acceleration values
        //

        int pvalue[3];

        ZVerify(SystemParametersInfo(SPI_GETMOUSE, 0, pvalue, 0));

        m_threshold1   = pvalue[0];
        m_threshold2   = pvalue[1];
        m_acceleration = pvalue[2];
        m_sensitivity  = 1.0f;

        /* !!! this only works on NT50
        int speed;
        ZVerify(SystemParametersInfo(SPI_GETMOUSESPEED, 0, &speed, 0));

        if (speed <= 2) {
           m_sensitivity = float(speed) / 32.0f;
        } else if(speed >= 3 && speed <= 10 ) {
           m_sensitivity = float(speed-2) / 8.0f; 
        } else {
           m_sensitivity = float(speed-6) / 4.0f;
        }
        */
    }

    void SetupDevice() 
    {
        //
        // Set the data format
        //

        DDCall(m_pdid->SetDataFormat(g_pdfDIMouse));

        //
        // Make some buffer space
        //

        if (m_bBuffered) {
            DIPROPDWORD dipdw; 

            dipdw.diph.dwSize       = sizeof(DIPROPDWORD); 
            dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
            dipdw.diph.dwObj        = 0; 
            dipdw.diph.dwHow        = DIPH_DEVICE; 
            dipdw.dwData            = 32; 

            DDCall(m_pdid->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph));
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Methods
    //
    //////////////////////////////////////////////////////////////////////////////
    
    void DoClip()
    {
        if (
               m_rect.XMax() > m_rect.XMin()
            && m_rect.YMax() > m_rect.YMin()
        ) {
            if (m_point.X() < m_rect.XMin()) {
                m_point.SetX(m_rect.XMin());
            }

            if (m_point.X() >= m_rect.XMax()) {
                m_point.SetX(m_rect.XMax() - 1);
            }

            if (m_point.Y() < m_rect.YMin()) {
                m_point.SetY(m_rect.YMin());
            }

            if (m_point.Y() >= m_rect.YMax()) {
                m_point.SetY(m_rect.YMax() - 1);
            }
        }
    }

    float CalculateDelta(int delta)
    {
        if (abs(delta) > m_threshold1 && m_acceleration >= 1) {
            if (abs(delta) > m_threshold2 && m_acceleration >= 2) {
                return float(delta) * 4.0f * m_sensitivity;
            } else {
                return float(delta) * 2.0f * m_sensitivity;
            }
        }

        return float(delta) * m_sensitivity;
    }

    void DeltaPosition(int& dx, int& dy)
    {
        if (dx != 0 || dy != 0) {
            //ZDebugOutput("MouseMove: (" + ZString(dx) + ", " + ZString(dy) + ")\n");

            m_point.SetX(m_point.X() + CalculateDelta(dx));
            m_point.SetY(m_point.Y() - CalculateDelta(dy));

            dx = 0;
            dy = 0;

            //
            // Clip to the screen rect if required
            //

            DoClip();

            //
            // Update outputs
            //

            m_vvalueObject[0]->GetValue()->SetValue(m_point.X());
            m_vvalueObject[1]->GetValue()->SetValue(m_point.Y());
        }
    }

    void DeltaWheel(int dz)
    {
        if (dz != 0 ) {
            //ZDebugOutput("MouseDZ: " + ZString(dz) + "\n");
            m_z += float(dz);

            if ((int)m_vvalueObject.size() >= 3) {
                m_vvalueObject[0]->GetValue()->SetValue(m_z);
            }
        }
    }

    void ButtonChanged(int index, bool bDown)
    {
        //ZDebugOutput("MouseButton: " + ZString(index) + (bDown ? " down" : " up") + "\n");

        m_vbuttonObject[index]->GetValue()->SetValue(bDown);
        m_pbuttonEventSource->Trigger(ButtonEventData(index, bDown));
    }

    void UpdateBuffered()
    {
        //
        // Get the data
        //

        DIDEVICEOBJECTDATA didod;
        DWORD count = 1;
        int dx = 0;
        int dy = 0;

        while (count == 1) {
            HRESULT hr = m_pdid->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), &didod, &count, 0);

            if (FAILED(hr)) {
                return;
            }

            if (count == 1) {
                //
                // Unpack the data
                //

                switch  (didod.dwOfs) {
                    case DIMOFS_BUTTON0: 
                        DeltaPosition(dx, dy);
                        ButtonChanged(0, ((didod.dwData & 0x80) != 0)); 
                        break;

                    case DIMOFS_BUTTON1: 
                        ButtonChanged(1, ((didod.dwData & 0x80) != 0)); 
                        break;

                    case DIMOFS_BUTTON2: 
                        ButtonChanged(2, ((didod.dwData & 0x80) != 0)); 
                        break;

                    case DIMOFS_BUTTON3: 
                        ButtonChanged(3, ((didod.dwData & 0x80) != 0)); 
                        break;

                    case DIMOFS_X:
                        dx += int(didod.dwData);
                        break;

                    case DIMOFS_Y:
                        dy += int(didod.dwData);
                        break;

                    case DIMOFS_Z:
                        DeltaWheel(int(didod.dwData));
                        break;
                }
            }

            //
            // do the mouse change
            //

            DeltaPosition(dx, dy);
        }
    }

    void UpdatePolled()
    {
        //
        // Poll the device
        //

        m_pdid->Poll();

        //
        // Get the data
        //

        DIMOUSESTATE dims;
        DDCall(m_pdid->GetDeviceState(sizeof(dims), &dims));

        //
        // Unpack the data
        //

        int dx = int(dims.lX);
        int dy = int(dims.lY);
        int dz = int(dims.lZ);

        DeltaPosition(dx, dy);
        DeltaWheel(dz);

        int count = (int)m_vbuttonObject.size();

        for (int index = 0; index < count; index++) {
            bool               bDown = ((dims.rgbButtons[index] & 0x80) != 0);
            ModifiableBoolean* pbool = m_vbuttonObject[index]->GetValue();

            if (bDown != pbool->GetValue()) {
                pbool->SetValue(bDown);
                ButtonChanged(index, bDown);
            }
        }
    }

    void Update()
    {
        //ZDebugOutput("Mouse Update\n");

        if (m_bEnabled) {
            //ZDebugOutput("Mouse Enabled\n");
            HRESULT hr = m_pdid->Acquire();

            //
            // We have to handle the case where another app has captured the
            // mouse, since we may have been switched out.
            //

            if (hr != DIERR_OTHERAPPHASPRIO) {
                DDCall(hr);

                if (m_bBuffered) {
                    UpdateBuffered();
                } else {
                    UpdatePolled();
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // MouseInputStream 
    //
    //////////////////////////////////////////////////////////////////////////////
    
    void SetClipRect(const Rect& rect) override
    {
        m_rect = rect;
        DoClip();
    }

    void SetPosition(const Point& point) override
    {
        m_point = point;
        DoClip();
    }

    void SetWheelPosition(float pos) override
    {
        m_z = pos;
    }

    const Point& GetPosition() override
    {
        return m_point;
    }

    void SetEnabled(bool bEnabled) override
    {
        if (m_bEnabled != bEnabled) {
            m_bEnabled = bEnabled;

            if (m_bEnabled) {
                //DDCall(m_pdid->SetCooperativeLevel(hwnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND));
            } else {
                //DDCall(m_pdid->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND));
                DDCall(m_pdid->Unacquire());
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // InputStream
    //
    //////////////////////////////////////////////////////////////////////////////
    
    int GetValueCount() override
    {
        return (int)m_vvalueObject.size();
    }

    int GetButtonCount() override
    {
        return (int)m_vbuttonObject.size();
    }

    Boolean* IsDown(int id) override
    {
        if (id < (int)m_vbuttonObject.size()) {
            return m_vbuttonObject[id]->GetValue();
        } else {
            return nullptr;
        }
    }

    Number* GetValue(int id) override
    {
        if (id < (int)m_vvalueObject.size()) {
            return m_vvalueObject[id]->GetValue();
        } else {
            return nullptr;
        }
    }

    ButtonEvent::Source* GetEventSource() override
    {
        return m_pbuttonEventSource;
    }
};

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

class InputEngineImpl : public InputEngine {
private:
    //////////////////////////////////////////////////////////////////////////////
    //
    // data members
    //
    //////////////////////////////////////////////////////////////////////////////

    HWND                                    m_hwnd;
    bool                                    m_bFocus;
    TRef<IDirectInput8>                     m_pdi;
    TRef<MouseInputStreamImpl>              m_pmouseInputStream;

public:
    //////////////////////////////////////////////////////////////////////////////
    //
    // Constructor
    //
    //////////////////////////////////////////////////////////////////////////////

    InputEngineImpl(HWND hwnd) :
        m_hwnd(hwnd),
        m_bFocus(false)
    {
        //
        // Create the direct input object
        //

        DDCall(DirectInput8Create(
            GetModuleHandle(nullptr),
            DIRECTINPUT_VERSION,
            IID_IDirectInput8,
            (void**)&m_pdi,
            nullptr
        ));

        g_pdfDIMouse = &c_dfDIMouse;

        //
        // If we failed then exit the app
        //

        if (m_pdi == nullptr) {
            ::MessageBox(nullptr, "Error initializing DirectInput.  Check your installation", "Error", MB_OK);
            _exit(0);
        }

        //
        // The mouse: the system mouse, which merges every attached pointing
        // device (as the first DI8DEVTYPE_MOUSE that enumeration found used to)
        //

        TRef<IDirectInputDevice8> pdid;

        DDCall(m_pdi->CreateDevice(GUID_SysMouse, &pdid, nullptr));

        if (pdid != nullptr) {
            m_pmouseInputStream = new MouseInputStreamImpl(pdid, m_hwnd);
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // InputEngine methods
    //
    //////////////////////////////////////////////////////////////////////////////

    MouseInputStream* GetMouse() override
    {
        return m_pmouseInputStream;
    }

    void Update() override
    {
        if (m_bFocus && m_pmouseInputStream) {
            m_pmouseInputStream->Update();
        }
    }

    void SetFocus(bool bFocus) override
    {
        m_bFocus = bFocus;
    }
};

TRef<InputEngine> CreateInputEngine(HWND hwnd)
{
    return new InputEngineImpl(hwnd);
}