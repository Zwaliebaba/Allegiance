//
// RedbookSound.h
//
// SoundEngine support for redbook audio
//

namespace SoundEngine {

//
// A generic disk player wrapper
//

class IDiskPlayer : public ISoundObject
{
public:
    // plays one track
    virtual HRESULT Play(int nTrack) = 0;

    // stops the CD player
    virtual HRESULT Stop() = 0;

    // returns S_OK if the CD player is playing, S_FALSE otherwise
    virtual HRESULT IsPlaying() = 0;

    // returns the current track of the CD player
    virtual HRESULT GetCurrentTrack(int& nTrack) = 0;

    // sets the gain on the CD player, from 0 to -100 dB
    virtual HRESULT SetGain(float fGain) = 0;
};


//
// Helper functions
//

// Creates the music player. It plays the WAVs listed in strFolder\tracks.lst
// (the artwork's Music folder): track n of the original Audio CD is line
// n - 1 (track 1 was the data track). Fails if there is no track list.
HRESULT CreateDiskPlayer(TRef<IDiskPlayer>& pdiskplayer, const ZString& strFolder);

// Creates a new disk player object that only has stubs for each of the calls
HRESULT CreateDummyDiskPlayer(TRef<IDiskPlayer>& pdiskplayer);

// creates a sound template representing a redbook audio track
HRESULT CreateRedbookSoundTemplate(TRef<ISoundTemplate>& pstDest, TRef<IDiskPlayer> diskPlayer, int nTrack);

};