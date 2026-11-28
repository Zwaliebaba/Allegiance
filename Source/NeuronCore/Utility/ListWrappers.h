#ifndef _listwrappers_h_
#define _listwrappers_h_

//////////////////////////////////////////////////////////////////////////////
//
// ListEventSource
//
//////////////////////////////////////////////////////////////////////////////

class ListEventSource
{
    TRef<EventSourceImpl> m_peventSource;

public:
    ListEventSource()
        : m_peventSource(new EventSourceImpl()) {};

    ListEventSource(const ListEventSource& les)
        : m_peventSource(les.m_peventSource) {};

    void operator () ()
        { m_peventSource->Trigger(); };

    virtual IEventSource* GetEvent()
    {
        return m_peventSource;
    }
};

//////////////////////////////////////////////////////////////////////////////
//
// Slist_utlListWrapper
//
//////////////////////////////////////////////////////////////////////////////

// these could be implemented much more efficently if it's useful to do so
template <class T>
class Slist_utlListWrapper : public List, public Slist_utl<T, ListEventSource> {
public:

    typedef Slink_utl<T, ListEventSource> Link;

    int           GetCount() override
    {
        // review: we could change this to O(1)
        int nCount = 0;
// VSNet 2003 port: resolve error C2475
#if _MSC_VER >= 1300
        for (Slink_utl<T, ListEventSource>* link = this->first(); link != nullptr; link = (link->next)())
#else
        for (Slink_utl<T, ListEventSource>* link = this->first(); link != NULL; link = link->next)
#endif
            nCount++;

        return nCount;
    }

    ItemID        GetItem(int index) override
    {
        return (*this)[index];
    }

    int           GetIndex(ItemID pitem) override
    {
        int nIndex = 0;
// VSNet 2003 port: resolve error C2475
#if _MSC_VER >= 1300
        for (Link* link = this->first(); link != nullptr; link = (link->next)())
#else
        for (Link* link = this->first(); link != NULL; link = link->next)
#endif
        {
            if ((ItemID)link == pitem)
                return nIndex;
            nIndex++;
        }

        return -1;
    }

    ItemID        GetNext(ItemID pitem) override
    {
        return ((Link*)pitem)->next();
    }

    IEventSource* GetChangedEvent() override
    {
        return this->sink.GetEvent();
    }
};


//////////////////////////////////////////////////////////////////////////////
//
// IntItemIDWrapper
//
//////////////////////////////////////////////////////////////////////////////

// unfortunately, ID=0 corresponds to NULL, so we can't use the default casts
// between ItemID and things like ShipID, since 0 is a valid ShipID
template <class T>
class IntItemIDWrapper
{
    T   m_t;

    enum { ZeroValue = ~(1 << (sizeof(T)*8 - 2)) };
public:
    IntItemIDWrapper() {}
    IntItemIDWrapper(T t) : m_t(t) {}
    IntItemIDWrapper(ItemID pitem) { m_t = ((INT_PTR)pitem == ZeroValue) ? 0 : (T)(INT_PTR)pitem; }

    IntItemIDWrapper& operator = (T t) { m_t = t; return *this; }
    operator T () { return m_t; }
    operator ItemID () { return (ItemID)(INT_PTR)((m_t == 0) ? (T)ZeroValue : m_t); };

    bool operator == (IntItemIDWrapper<T> t) const { return m_t == t.m_t; }
    bool operator <= (IntItemIDWrapper<T> t) const { return m_t <= t.m_t; }
    bool operator >= (IntItemIDWrapper<T> t) const { return m_t >= t.m_t; }
    bool operator <  (IntItemIDWrapper<T> t) const { return m_t < t.m_t; }
    bool operator >  (IntItemIDWrapper<T> t) const { return m_t > t.m_t; }
};

//////////////////////////////////////////////////////////////////////////////
//
// TListListWrapper
//
//////////////////////////////////////////////////////////////////////////////

// these could be implemented much more efficently if it's useful to do so
template <
    class T,     
    class EqualsType  = DefaultEquals,
    class CompareType = DefaultNoCompare
>
class TListListWrapper : public List, public TList<T, EqualsType, CompareType, ListEventSource> {
public:
    // The base's members, for two-phase lookup (/permissive-)
    typedef TList<T, EqualsType, CompareType, ListEventSource> ListType;
    typedef typename ListType::Iterator Iterator;
    using ListType::GetSink;

    TListListWrapper()
    {
    }

    TListListWrapper(EqualsType equals, CompareType compare)
        : TList<T, EqualsType, CompareType, ListEventSource>(equals, compare)
    {
    }

    int           GetCount() override
    {
        return TList<T, EqualsType, CompareType, ListEventSource>::GetCount();
    }

    ItemID        GetItem(int index) override
    {
        if (index < 0 || index >= GetCount())
            return nullptr;
        else
            return (ItemID)((*this)[index]);
    }

    int           GetIndex(ItemID pitem) override
    {
        // review: returning the key may be more efficient than returning the value.
        int nIndex = 0;

        for (Iterator iter(*this); !iter.End(); iter.Next())
        {
            if ((ItemID)iter.Value() == pitem)
                return nIndex;
            nIndex++;
        }

        return -1;
    }

    ItemID        GetNext(ItemID pitem) override
    {
        for (Iterator iter(*this); !iter.End(); iter.Next())
        {
            if ((ItemID)iter.Value() == pitem)
            {
                iter.Next();

                if (iter.End())
                    return nullptr;
                else
                    return (ItemID)iter.Value();
            }
        }

        return nullptr;
    }

    IEventSource* GetChangedEvent() override
    {
        return GetSink().GetEvent();
    }
};


//////////////////////////////////////////////////////////////////////////////
//
// TMapListWrapper
//
//////////////////////////////////////////////////////////////////////////////

// these could be implemented much more efficently if it's useful to do so
template <class T, class U>
class TMapListWrapper : public List, public TMap<T, U, ListEventSource> {
public:
    // The base's members, for two-phase lookup (/permissive-)
    typedef TMap<T, U, ListEventSource> MapType;
    typedef typename MapType::Iterator Iterator;
    using MapType::Count;
    using MapType::GetSink;

    int           GetCount() override
    {
        return Count();
    }

    ItemID        GetItem(int index) override
    {
        Iterator iter(*this);

        for (int currentIndex = 0; !iter.End() && currentIndex < index; currentIndex++)
        {
            iter.Next();
        }

        if (iter.End())
            return nullptr;
        else
            return (ItemID)iter.Value();
    }

    int           GetIndex(ItemID pitem) override
    {
        // review: returning the key may be more efficient than returning the value.
        int nIndex = 0;

        for (Iterator iter(*this); !iter.End(); iter.Next())
        {
            if ((ItemID)iter.Value() == pitem)
                return nIndex;
            nIndex++;
        }

        return -1;
    }

    ItemID        GetNext(ItemID pitem) override
    {
        for (Iterator iter(*this); !iter.End(); iter.Next())
        {
            if ((ItemID)iter.Value() == pitem)
            {
                iter.Next();

                if (iter.End())
                    return nullptr;
                else
                    return (ItemID)iter.Value();
            }
        }

        return nullptr;
    }

    IEventSource* GetChangedEvent() override
    {
        return GetSink().GetEvent();
    }
};


///////////////////////////////////////////////////////////////////////////////
//
// FilteredList
//
///////////////////////////////////////////////////////////////////////////////

template <class Filter>
class FilteredList : public List
{
    TRef<List> m_list;
    Filter m_filter;

public:

    FilteredList(List* list)
        : m_list(list) {};

    FilteredList(List* list, Filter filter)
        : m_list(list), m_filter(filter) {};

    void SetFilter(Filter filter)
    {
        m_filter = filter;
    }

    int           GetCount() override
    {
        // this could be O(n) if we hooked the "changed" event
        int nCount = 0;
        ItemID currentItem = m_list->GetItem(0);

        while (currentItem != nullptr)
        {
            if (m_filter(currentItem))
                nCount++;
            currentItem = m_list->GetNext(currentItem);
        }

        return nCount;
    }

    ItemID        GetItem(int index) override
    {
        int nCurrentIndex = 0;
        ItemID currentItem = m_list->GetItem(0);

        while (currentItem != nullptr)
        {
            if (m_filter(currentItem))
            {
                if (nCurrentIndex == index)
                    break;
                else
                    nCurrentIndex++;
            }
            currentItem = m_list->GetNext(currentItem);
        }

        return currentItem;
    }

    int           GetIndex(ItemID pitem) override
    {
        // review: returning the key may be more efficient than returning the value.
        int nIndex = 0;
        ItemID currentItem = m_list->GetItem(0);

        while (currentItem != nullptr)
        {
            if (m_filter(currentItem))
            {
                if (currentItem == pitem)
                    return nIndex;
                else
                    nIndex++;
            }
            currentItem = m_list->GetNext(currentItem);
        }

        return -1;
    }

    ItemID        GetNext(ItemID pitem) override
    {
        ItemID currentItem = m_list->GetNext(pitem);

        while (currentItem != nullptr)
        {
            if (m_filter(currentItem))
            {
                return currentItem;
            }
            currentItem = m_list->GetNext(currentItem);
        }

        return nullptr;
    }

    IEventSource* GetChangedEvent() override
    {
        return m_list->GetChangedEvent();
    }
};


///////////////////////////////////////////////////////////////////////////////
//
// ListDelegate
//
///////////////////////////////////////////////////////////////////////////////
class ListDelegate : public List
{
private:
    List* m_plist;
    TRef<IObject> m_pcontainer;

public:

    ListDelegate(List* plist, IObject* pcontainer = nullptr)
        : m_plist(plist), m_pcontainer(pcontainer) {}

    int           GetCount() override
    {
        return m_plist->GetCount();
    }

    ItemID        GetItem(int index) override
    {
        return m_plist->GetItem(index);
    }

    int           GetIndex(ItemID pitem) override
    {
        return m_plist->GetIndex(pitem);
    }

    ItemID        GetNext(ItemID pitem) override
    {
        return m_plist->GetNext(pitem);
    }

    IEventSource* GetChangedEvent() override
    {
        return m_plist->GetChangedEvent();
    }
};




///////////////////////////////////////////////////////////////////////////////
//
// ConcatinatedList
//
///////////////////////////////////////////////////////////////////////////////

class ConcatinatedList : public List, public IEventSink
{
    TRef<List> m_list1;
    TRef<List> m_list2;
    ListEventSource m_pfnChanged;
    TRef<IEventSink> m_sinkDelegate;

public:

    ConcatinatedList(List* list1, List* list2)
        : m_list1(list1), m_list2(list2) 
    {
        // add this to the changed notifications of the sublists
        m_sinkDelegate = IEventSink::CreateDelegate(this);
        m_list1->GetChangedEvent()->AddSink(m_sinkDelegate);
        m_list2->GetChangedEvent()->AddSink(m_sinkDelegate);
    };

    ~ConcatinatedList()
    {
        m_list1->GetChangedEvent()->RemoveSink(m_sinkDelegate);
        m_list2->GetChangedEvent()->RemoveSink(m_sinkDelegate);
    }

    int           GetCount() override
    {
        return m_list1->GetCount() + m_list2->GetCount();
    }

    ItemID        GetItem(int index) override
    {
        int nList1Count = m_list1->GetCount();

        if (index < nList1Count)
            return m_list1->GetItem(index);
        else
            return m_list2->GetItem(index - nList1Count);
    }

    int           GetIndex(ItemID pitem) override
    {
        int index = m_list1->GetIndex(pitem);

        if (index == -1)
        {
            index = m_list2->GetIndex(pitem);

            if (index != -1) 
                index += m_list1->GetCount();
        }

        return index;
    }

    ItemID        GetNext(ItemID pitem) override
    {
        // case 1: pitem and pitem->next are in list1
        ItemID next = m_list1->GetNext(pitem);

        if (next == nullptr)
        {
            // case 2: pitem and pitem->next are in list2
            next = m_list2->GetNext(pitem);

            if ((next == nullptr) && (m_list1->GetIndex(pitem) != -1))
            {
                // case 3: pitem is in list1, pitem next is in list2
                next = m_list2->GetItem(0);
            }
        }

        return next;
    }

    IEventSource* GetChangedEvent() override
    {
        return m_pfnChanged.GetEvent();
    }

    bool OnEvent(IEventSource* pevent) override
    {
        m_pfnChanged();
        return true;
    }
};



///////////////////////////////////////////////////////////////////////////////
//
// SortedList
//
///////////////////////////////////////////////////////////////////////////////

typedef bool (*ItemIDCompareFunction)(ItemID id1, ItemID id2);

template <class CompareFunctionType>
class SortedList : public List, public IEventSink
{
    // a sorted list of the items in question
    typedef TList<ItemID, DefaultEquals, CompareFunctionType> SortedListType;
    SortedListType m_listSorted;
    
    TRef<List> m_list;
    bool m_fNeedsSort;
    ListEventSource m_pfnChanged;
    TRef<IEventSink> m_sinkDelegate;

    void CheckSorting()
    {
        if (m_fNeedsSort)
        {
            m_listSorted.SetEmpty();

            // use an insertion sort - O(n^2), but it's a stable sort
            ItemID pitem = m_list->GetItem(0);

            while (pitem) {
                m_listSorted.InsertSorted(pitem);

                pitem = m_list->GetNext(pitem);
            }
            
            m_fNeedsSort = false;
        }
    }

public:

    SortedList(List* list, CompareFunctionType fnCompare)
        : m_list(list), m_listSorted(DefaultEquals(), fnCompare), m_fNeedsSort(true)
    {
        // add this to the changed notifications of the unsorted list
        m_sinkDelegate = IEventSink::CreateDelegate(this);
        m_list->GetChangedEvent()->AddSink(m_sinkDelegate);
    };

    ~SortedList()
    {
        m_list->GetChangedEvent()->RemoveSink(m_sinkDelegate);
    }

    int           GetCount() override
    {
        return m_list->GetCount();
    }

    ItemID        GetItem(int index) override
    {
        CheckSorting();

        typename SortedListType::Iterator iter(m_listSorted);

        for (int currentIndex = 0; !iter.End() && currentIndex < index; currentIndex++)
        {
            iter.Next();
        }

        if (iter.End())
            return nullptr;
        else
            return (ItemID)iter.Value();
    }

    int           GetIndex(ItemID pitem) override
    {
        CheckSorting();

        int nIndex = 0;

        for (typename SortedListType::Iterator iter(m_listSorted); !iter.End(); iter.Next())
        {
            if ((ItemID)iter.Value() == pitem)
                return nIndex;
            nIndex++;
        }

        return -1;
    }

    ItemID        GetNext(ItemID pitem) override
    {
        CheckSorting();

        for (typename SortedListType::Iterator iter(m_listSorted); !iter.End(); iter.Next())
        {
            if ((ItemID)iter.Value() == pitem)
            {
                iter.Next();

                if (iter.End())
                    return nullptr;
                else
                    return (ItemID)iter.Value();
            }
        }

        return nullptr;
    }

    IEventSource* GetChangedEvent() override
    {
        return m_pfnChanged.GetEvent();
    }

    bool OnEvent(IEventSource* pevent) override
    {
        m_fNeedsSort = true;
        m_pfnChanged();
        return true;
    }
};



///////////////////////////////////////////////////////////////////////////////
//
// SingletonList
//
///////////////////////////////////////////////////////////////////////////////

class SingletonList : public List
{
    ListEventSource m_pfnChanged;
    ItemID m_itemID;

public:

    SingletonList(ItemID itemID = nullptr)
        : m_itemID(itemID)
    {
    }

    void SetItem(ItemID itemID)
    {
        m_itemID = itemID;
        m_pfnChanged();
    }

    //
    // List interface:
    //

    int           GetCount() override
    {
        return 1;
    }

    ItemID        GetItem(int index) override
    {
        if (index == 0)
            return m_itemID;
        else
            return nullptr;
    }

    int           GetIndex(ItemID pitem) override
    {
        if (pitem = m_itemID)
            return 0;
        else 
            return -1;
    }

    ItemID        GetNext(ItemID pitem) override
    {
        return nullptr;
    }

    IEventSource* GetChangedEvent() override
    {
        return m_pfnChanged.GetEvent();
    }
};



#endif
