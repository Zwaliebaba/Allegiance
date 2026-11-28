#ifndef _genericlist_h_
#define _genericlist_h_

//////////////////////////////////////////////////////////////////////////////
//
// List Item Identifier
//
//////////////////////////////////////////////////////////////////////////////

typedef void* ItemID;

//////////////////////////////////////////////////////////////////////////////
//
// List
//
//////////////////////////////////////////////////////////////////////////////

class List : public IObject {
public:
    virtual int           GetCount()             = 0;
    virtual ItemID        GetItem(int index)     = 0;
    virtual int           GetIndex(ItemID pitem) = 0;
    virtual ItemID        GetNext(ItemID pitem)  = 0;
    virtual IEventSource* GetChangedEvent()      = 0;
};

class EmptyList : public List {
public:
    int           GetCount() override             { return 0;                        }
    ItemID        GetItem(int index) override     { return NULL;                     }
    int           GetIndex(ItemID pitem) override { return -1;                       }
    ItemID        GetNext(ItemID pitem) override  { return NULL;                     }
    IEventSource* GetChangedEvent() override      { return IEventSource::GetNever(); }
};

#endif