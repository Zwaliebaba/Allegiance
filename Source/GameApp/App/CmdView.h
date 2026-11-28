#ifndef	TREKGEO_H
#define	TREKGEO_H

/////////////////////////////////////////////////////////////////////////////
//
// ImageStringPane 
//
/////////////////////////////////////////////////////////////////////////////

class ImageStringPane : public Pane
{
private:
    TRef<StringPane> m_pStringPane;
    
public:
    enum {
        ImageLeft = 0,
        ImageRight = 1
    };

    ImageStringPane(LPCSTR szText, LPCSTR szImage, bool bUp = true, int Layout = ImageLeft);
    void UpdateLayout() override;
    virtual void SetString(LPCSTR szText);

};

/////////////////////////////////////////////////////////////////////////////
//
// ImageStringButtonPane 
//
/////////////////////////////////////////////////////////////////////////////

class ImageStringButtonPane : public Pane
{
private:
    TRef<ImageStringPane> m_pImageStringPaneUp;
    TRef<ImageStringPane> m_pImageStringPaneDn;
    TRef<ButtonPane> m_pButtonPane;

public:

    ImageStringButtonPane(LPCSTR szText, LPCSTR szImageUp, LPCSTR szImageDn, int Layout = ImageStringPane::ImageLeft);
    void UpdateLayout() override;
    virtual IEventSource* GetEventSource();
    virtual void SetString(LPCSTR szText);

};


//////////////////////////////////////////////////////////////////////////////
//
// CommandGeo
//
//////////////////////////////////////////////////////////////////////////////

#define CmdViewMaxIconScreenSize  7.0f

class CommandGeo : public Geo
{
private:
    std::vector<VertexL>  m_vertices;
    std::vector<WORD>     m_indices;
    TRef<IclusterIGC> m_pcluster;
    float             m_zGrid;

    void DrawShips(Context* pcontext);

public:
    CommandGeo(float radius, float zGrid, int nSegments);
    void Render(Context* pcontext) override;
    void SetCluster(IclusterIGC* pcluster);

    //
    // Value members
    //

    ZString GetFunctionName() override { return "CommandGeo"; }

    static const Color s_colorGrid;
    static const Color s_colorGridMajor;
    static const Color s_colorDropLineUp;
    static const Color s_colorDropLineDown;
    static const Color s_colorFeet;
    static const Color s_colorNeutral;
};


#endif
