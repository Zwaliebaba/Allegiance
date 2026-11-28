#include "pch.h"

/////////////////////////////////////////////////////////////////////////////
//
// PaneImage
//
/////////////////////////////////////////////////////////////////////////////

class PaneImage : public Image {
private:
    /////////////////////////////////////////////////////////////////////////////
    //
    // Types
    //
    /////////////////////////////////////////////////////////////////////////////

    class ImageTopPaneSiteImpl : public TopPaneSite {
    private:
        PaneImage* m_pimage;

    public:
        ImageTopPaneSiteImpl(PaneImage* pimage) :
            m_pimage(pimage)
        {
        }

        Point TransformLocalToImage(const Point& point) override
        {
            return m_pimage->TransformLocalToImage(point);
        }

        void SurfaceChanged() override
        {
            m_pimage->PaneChanged();
        }

        void SizeChanged() override
        {
            m_pimage->PaneChanged();
        }
    };

    /////////////////////////////////////////////////////////////////////////////
    //
    // Members
    //
    /////////////////////////////////////////////////////////////////////////////

    TRef<TopPane> m_ptopPane;
    Point         m_pointOrigin;

public:
    /////////////////////////////////////////////////////////////////////////////
    //
    // Constructor
    //
    /////////////////////////////////////////////////////////////////////////////

    PaneImage(Engine* pengine, SurfaceType stype, bool bColorKey, Pane* ppane)
    {
        m_ptopPane =
            new TopPane(
                pengine,
                stype,
                bColorKey,
                new ImageTopPaneSiteImpl(this),
                ppane
            );
    }

    /////////////////////////////////////////////////////////////////////////////
    //
    // Implementation methods
    //
    /////////////////////////////////////////////////////////////////////////////

    void PaneChanged()
    {
        Changed();
    }

    /////////////////////////////////////////////////////////////////////////////
    //
    // Image methods
    //
    /////////////////////////////////////////////////////////////////////////////

    void CalcBounds() override
    {
        m_bounds.SetRect(
            Rect(
                Point(0, 0),
                Point::Cast(m_ptopPane->GetSize())
            )
        );
    }

    void Render(Context* pcontext) override
    {
        ZEnter("PaneImage::Render()");

        pcontext->TransformLocalToImage(Vector(0, 0, 0), m_pointOrigin);
        pcontext->DrawImage(m_ptopPane->GetSurface());

        ZExit("PaneImage::Render()");
    }

    TRef<Surface> GetSurface() override
    {
        return m_ptopPane->GetSurface();
    }

    /////////////////////////////////////////////////////////////////////////////
    //
    // Value methods
    //
    /////////////////////////////////////////////////////////////////////////////

    void Evaluate() override
    {
        m_ptopPane->Evaluate();
        Image::Evaluate();
    }

    ZString GetFunctionName() override 
    { 
        return "PaneImage"; 
    }

    Point TransformLocalToImage(const Point& point)
    {
        return m_pointOrigin + point;
    }

    /////////////////////////////////////////////////////////////////////////////
    //
    // IMouseInput methods
    //
    /////////////////////////////////////////////////////////////////////////////

    void RemoveCapture() override
    {
        m_ptopPane->RemoveCapture();
    }

    MouseResult HitTest(IInputProvider* pprovider, const Point& point, bool bCaptured) override
    {
        return m_ptopPane->HitTest(pprovider, point, bCaptured);
    }

    void MouseMove(IInputProvider* pprovider, const Point& point, bool bCaptured, bool bInside) override
    {
        m_ptopPane->MouseMove(pprovider, point, bCaptured, bInside);
    }

    void MouseEnter(IInputProvider* pprovider, const Point& point) override
    {
        m_ptopPane->MouseEnter(pprovider, point);
    }

    void MouseLeave(IInputProvider* pprovider) override
    {
        m_ptopPane->MouseLeave(pprovider);
    }

    MouseResult Button(IInputProvider* pprovider, const Point& point, int button, bool bCaptured, bool bInside, bool bDown) override
    {
        return m_ptopPane->Button(pprovider, point, button, bCaptured, bInside, bDown);
    }
};

/////////////////////////////////////////////////////////////////////////////
//
// Constructor
//
/////////////////////////////////////////////////////////////////////////////

TRef<Image> CreatePaneImage(Engine* pengine, SurfaceType stype, bool bColorKey, Pane* ppane)
{
    return new PaneImage(pengine, stype, bColorKey, ppane);
}
