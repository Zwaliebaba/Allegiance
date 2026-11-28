#ifndef _BADWORDS_H_
#define _BADWORDS_H_

//------------------------------------------------------------------------------
// functions to handle bad words
//------------------------------------------------------------------------------
void    LoadBadWords (void);
void    BuildFilterAutomata (char* pBuffer);  // the decoded list: a header, then words each ended by "\r\n", then a space
ZString CensorBadWords (const ZString& string);
void    ToggleCensorDisplay (void);
bool    CensorDisplay (void);

//------------------------------------------------------------------------------

#endif  //  _BADWORDS_H_