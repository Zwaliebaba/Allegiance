#ifndef _tref_H_
#define _tref_H_

#include <type_traits>

//////////////////////////////////////////////////////////////////////////////
//
// Base class for reference counted objects
//
//////////////////////////////////////////////////////////////////////////////

class IObjectSingle {
private:
    DWORD m_count;

protected:
    typedef IObjectSingle QIType;


    void Internal_Release()
    {
        --m_count;
    }

public:
    IObjectSingle() :
        m_count(0)
    {
    }

    virtual ~IObjectSingle()
    {
    }


    DWORD GetCount() const
    { 
        return m_count;
    }

    virtual bool IsValid()
    {
        return true;
    }


#ifdef _DEBUG
    virtual
#endif
    DWORD __stdcall AddRef()
    {
        return ++m_count;
    }

#ifdef _DEBUG
    virtual
#endif
    DWORD __stdcall Release()
    {
        if (--m_count == 0) {
            delete this;
            return 0;
        }

        return m_count;
    }
};

//////////////////////////////////////////////////////////////////////////////
//
// Object Interface
//
//////////////////////////////////////////////////////////////////////////////

class IObject : virtual public IObjectSingle {
public:
};

//////////////////////////////////////////////////////////////////////////////
//
// Reference to a reference counted object
//
//////////////////////////////////////////////////////////////////////////////

template <class Type>
class TRef {
private:
    Type* m_pt;

public:
    //
    // constructors
    //

    TRef() :
        m_pt(nullptr)
    {
    }

    TRef(const Type* pt)
    {
        m_pt = (Type*)pt;
        if (m_pt)
            m_pt->AddRef();
    }

    TRef(const TRef& tref) :
        m_pt(tref.m_pt)
    {
        if (m_pt)
            m_pt->AddRef();
    }

    //
    // A TRef to a derived type converts to a TRef to its base. Through Type*
    // that took two user-defined conversions (TRef<Derived> -> Derived* and
    // Base* -> TRef<Base>), which only permissive mode accepted.
    //

    template <class OtherType,
              class = typename std::enable_if<std::is_convertible<OtherType*, Type*>::value>::type>
    TRef(const TRef<OtherType>& tref) :
        m_pt((OtherType*)tref)
    {
        if (m_pt)
            m_pt->AddRef();
    }

    //
    // assignment
    //

    TRef* Pointer()
    {
        return this;
    }

    Type** operator&()
    {
        if (m_pt) {
            m_pt->Release();
            m_pt = nullptr;
        }
        return &m_pt;
    }

    TRef& operator=(const TRef& tref)
    {
        Type* ptOld = m_pt;
        m_pt = tref.m_pt;

        if (m_pt)
            m_pt->AddRef();

        if (ptOld)
            ptOld->Release();

        return *this;
    }

    // Assign from a TRef to a derived type (see the converting constructor):
    // without this, both operator= overloads matched and the call was ambiguous.
    template <class OtherType,
              class = typename std::enable_if<std::is_convertible<OtherType*, Type*>::value>::type>
    TRef& operator=(const TRef<OtherType>& tref)
    {
        return *this = (OtherType*)tref;
    }

    TRef& operator=(const Type* ptNew)
    {
        Type* ptOld = m_pt;
        m_pt = (Type*)ptNew;

        if (m_pt)
            m_pt->AddRef();

        if (ptOld)
            ptOld->Release();

        return *this;
    }

    //
    // destructor
    //

    ~TRef(void)
    {
        if (m_pt)
            m_pt->Release();
    }

    //
    // members
    //

    operator Type*(void) const { return m_pt; }
    Type& operator*(void) const { return *m_pt; }
    Type* operator->(void) const { return m_pt; }

    friend bool operator==(const TRef& t1, Type* pt)
    {
        return t1.m_pt == pt;
    }

    //
    // TRef against TRef exactly: otherwise C++20's reversed candidates make
    // (TRef, Type*) and (Type*, TRef) equally good
    //

    friend bool operator==(const TRef& t1, const TRef& t2)
    {
        return t1.m_pt == t2.m_pt;
    }

    friend bool operator!=(const TRef& t1, const TRef& t2)
    {
        return t1.m_pt != t2.m_pt;
    }

    friend bool operator!=(const TRef& t1, Type* pt)
    {
        return t1.m_pt != pt;
    }


    // define comparison operators for use in associative containers
    inline friend bool operator<(const TRef& t1, Type* pt) { return t1.m_pt < pt; };
    inline friend bool operator>(const TRef& t1, Type* pt) { return t1.m_pt > pt; };
    inline friend bool operator<=(const TRef& t1, Type* pt) { return t1.m_pt <= pt; };
    inline friend bool operator>=(const TRef& t1, Type* pt) { return t1.m_pt >= pt; };
};

#endif
