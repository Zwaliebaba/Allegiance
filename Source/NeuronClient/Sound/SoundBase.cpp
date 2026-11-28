#include "pch.h"
#include "SoundBase.h"

//
// These includes are the real heart of this file.  They were all originally
// part of this file, but I spun them off into .h files when they became too
// big to be managable.
//

#include "SoundMath.h"
#include "X3DPan.h"
#include "Xa2Voice.h"
#include "VirtualBuffer.h"
#include "Xa2Engine.h"


namespace SoundEngine {


// create a default sound engine; fails without an audio device
HRESULT CreateSoundEngine(TRef<ISoundEngine>& psoundengine)
{
    TRef<XA2SoundEngine> psoundengineimpl = new XA2SoundEngine();

    HRESULT hr = psoundengineimpl->Init();

    if (FAILED(hr))
        return hr;

    psoundengine = psoundengineimpl;

    return S_OK;
}


#ifdef _DEBUG
// a helper function for trying to dump an arbitrary object
ZString SoundDebugDump(ISoundObject* pobject, const ZString& strIndent)
{
    ISoundDebugDump* pdebug = dynamic_cast<ISoundDebugDump*>(pobject);

    if (pdebug)
    {
        return pdebug->DebugDump(strIndent);
    }
    else
    {
        return strIndent + "<unknown>\n";
    }
}
#endif

};