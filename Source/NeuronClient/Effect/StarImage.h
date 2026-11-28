#ifndef _StarImage_h_
#define _StarImage_h_

//////////////////////////////////////////////////////////////////////////////
//
// StarImage header
//
//////////////////////////////////////////////////////////////////////////////

class StarImage : public Image {
protected:
    StarImage(Viewport* pviewport) :
        Image(pviewport)
    {
    }

public:
    static TRef<StarImage> Create(Viewport* pviewport, short count);

    virtual void SetCount(unsigned int seed, short count) = 0;
    void Render(Context* pcontext) override = 0;

    //
    // Value methods
    //

    ZString GetFunctionName() override { return "StarImage"; }
};


#endif
