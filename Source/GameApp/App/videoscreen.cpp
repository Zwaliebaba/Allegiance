#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// Video Screen
//
//////////////////////////////////////////////////////////////////////////////

class VideoScreen : 
    public Screen,
    public IEventSink,
    public IKeyboardInput
{
public:
    /////////////////////////////////////////////////////////////////////////////
    //
    // Types
    //
    /////////////////////////////////////////////////////////////////////////////

    class PickImage : public Image {
    private:
        VideoScreen* m_pobject;

    public:
        PickImage(VideoScreen* pobject) :
            m_pobject(pobject)
        {
        }

        MouseResult HitTest(IInputProvider* pprovider, const Point& point, bool bCaptured)
        {
            return MouseResultHit();
        }

        MouseResult Button(
            IInputProvider* pprovider,
            const Point& point,
            int button,
            bool bCaptured,
            bool bInside,
            bool bDown
        ) {
            if (bDown) {
                m_pobject->Picked();
            }
            return MouseResult();
        }
    };

    /////////////////////////////////////////////////////////////////////////////
    //
    // Data members
    //
    /////////////////////////////////////////////////////////////////////////////

    TRef<Engine>         m_pengine;
    TRef<WrapImage>      m_pwrapImageVideo;
    TRef<VideoImage>     m_pvideoImage;
    TRef<Image>          m_pimage;
    TRef<IKeyboardInput> m_pkeyboardInputOldFocus;
    int                  m_indexVideo;
    bool                 m_bNextVideo;

    /////////////////////////////////////////////////////////////////////////////
    //
    // Constructor
    //
    /////////////////////////////////////////////////////////////////////////////

    VideoScreen(Modeler* pmodeler) :
        m_pengine(pmodeler->GetEngine()),
        m_indexVideo(-1),
        m_bNextVideo(false)
    {
        //
        // Create the image hierarchy
        //

        m_pwrapImageVideo = new WrapImage(Image::GetEmpty());
        m_pimage =
            new GroupImage(
                new PickImage(this),
                m_pwrapImageVideo
            );

        m_pkeyboardInputOldFocus = GetWindow()->GetFocus();
        GetWindow()->SetFocus(IKeyboardInput::CreateDelegate(this));

        //
        // Start the first video
        //

        NextVideo();
    }

    /////////////////////////////////////////////////////////////////////////////
    //
    // Implementation methods
    //
    /////////////////////////////////////////////////////////////////////////////

    void Dismiss()
    {
        GetWindow()->SetFocus(m_pkeyboardInputOldFocus);
        GetWindow()->screen(ScreenIDIntroScreen);
    }

    void NextVideo()
    {
        ZString str;

        m_indexVideo++;
        switch (m_indexVideo) {
            // H.264/AAC MP4, converted once from the original Indeo AVIs.
            // (The intro movie that followed them was on the game CD, which
            // the repository doesn't have; it is no longer looked for.)
            case 0: str = "msr_games"; break;
            case 1: str = "logo1";     break;
            case 2: str = "logo2";     break;
            default: 
                return;
        };

        m_pvideoImage = 
            CreateVideoImage(
                m_pengine,
                GetWindow()->GetScreenRectValue(), 
                GetModeler()->GetArtFile(str, "mp4")
            );

        if (!m_pvideoImage->IsValid()) {
            NextVideo();
        } else {
            m_pwrapImageVideo->SetImage(m_pvideoImage);
            m_pvideoImage->GetEventSource()->AddSink(IEventSink::CreateDelegate(this));
        }
    }

    bool OnEvent(IEventSource* pevent)
    {
        m_bNextVideo = true;
        return false;
    }

    void Picked()
    {
        NextVideo();
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // IKeyboardInput
    //
    //////////////////////////////////////////////////////////////////////////////

    bool OnKey(IInputProvider* pprovider, const KeyState& ks, bool& fForceTranslate)
    {
        if (ks.bDown) {
            NextVideo();
        }
        return false;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Screen Methods
    //
    //////////////////////////////////////////////////////////////////////////////

    Image* GetImage()
    {
        return m_pimage;
    }

    WinPoint GetSize()
    {
        return WinPoint(640, 480);
    }

    void OnFrame() 
    {
        if (m_bNextVideo) {
            m_bNextVideo = false;
            NextVideo();
        }
        if (m_indexVideo > 2) {
            Dismiss();
        }
    }
};

//////////////////////////////////////////////////////////////////////////////
//
// Constructor
//
//////////////////////////////////////////////////////////////////////////////

TRef<Screen> CreateVideoScreen(Modeler* pmodeler)
{
    return new VideoScreen(pmodeler);
}
