#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// VideoImage
//
// Played the intro AVIs through the DirectShow multimedia streaming API
// (IAMMultiMediaStream with a DirectDraw media stream), which needs a
// DirectDraw surface to decode into. With DirectDraw gone the image reports
// the movie as finished on its second frame (the original also skipped the
// first), so the screens that play movies move straight on. Playing them
// again is a follow-up (Media Foundation into a system-memory surface).
//
//////////////////////////////////////////////////////////////////////////////

class VideoImageImpl : public VideoImage {
public:
    RectValue* GetRect() { return RectValue::Cast(GetChild(0)); }

    TRef<EventSourceImpl> m_peventSource;
    ZString               m_strFile;
    bool                  m_bTriggered;
    bool                  m_bFirstFrame;

    VideoImageImpl(
        RectValue*     prect, 
        const ZString& str
    ) :
        VideoImage(prect),
        m_peventSource(new EventSourceImpl()),
        m_strFile(str),
        m_bTriggered(false),
        m_bFirstFrame(true)
    {
    }

    void End()
    {
        if (!m_bTriggered) {
            m_bTriggered = true;
            m_peventSource->Trigger();
        }
    }

    IEventSource* GetEventSource()
    {
        return m_peventSource;
    }

    void CalcBounds()
    {
        m_bounds.SetRect(GetRect()->GetValue());
    }

    void Render(Context* pcontext)
    {
        if (m_bFirstFrame) {
            m_bFirstFrame = false;
            return;
        }

        End();
    }

    ZString GetFunctionName() { return "VideoImage"; }
};

//////////////////////////////////////////////////////////////////////////////
//
// 
//
//////////////////////////////////////////////////////////////////////////////

TRef<VideoImage> CreateVideoImage(
    Engine*        pengine,
    RectValue*     prect, 
    const ZString& str
) {
    return new VideoImageImpl(prect, str);
}
