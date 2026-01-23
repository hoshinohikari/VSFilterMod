/*
 * Rendering cache utilities (ported from MPC-HC)
 */

#pragma once

#include <atlcoll.h>
#include <cmath>
#include <memory>
#include "STS.h"

inline bool IsNearlyEqual(double a, double b, double epsilon = 1e-6)
{
    return std::fabs(a - b) <= epsilon;
}

template<typename K, typename V, class KTraits = CElementTraits<K>, class VTraits = CElementTraits<V>>
class CRenderingCache : private CAtlMap<K, POSITION, KTraits>
{
private:
    size_t m_maxSize;
    struct CPositionValue
    {
        POSITION pos;
        V value;
    };
    CAtlList<CPositionValue> m_list;

public:
    CRenderingCache(size_t maxSize)
        : m_maxSize(maxSize)
    {
    }

    bool Lookup(typename KTraits::INARGTYPE key, typename VTraits::OUTARGTYPE value)
    {
        POSITION pos = nullptr;
        bool bFound = __super::Lookup(key, pos);

        if(bFound)
        {
            m_list.MoveToHead(pos);
            value = m_list.GetHead().value;
        }

        return bFound;
    }

    POSITION SetAt(typename KTraits::INARGTYPE key, typename VTraits::INARGTYPE value)
    {
        POSITION pos = nullptr;
        bool bFound = __super::Lookup(key, pos);

        if(bFound)
        {
            m_list.MoveToHead(pos);
            CPositionValue& posVal = m_list.GetHead();
            pos = posVal.pos;
            posVal.value = value;
        }
        else
        {
            if(m_list.GetCount() >= m_maxSize)
            {
                __super::RemoveAtPos(m_list.GetTail().pos);
                m_list.RemoveTailNoReturn();
            }
            pos = __super::SetAt(key, m_list.AddHead());
            CPositionValue& posVal = m_list.GetHead();
            posVal.pos = pos;
            posVal.value = value;
        }

        return pos;
    }

    void Clear()
    {
        m_list.RemoveAll();
        __super::RemoveAll();
    }
};

template <class Key>
class CKeyTraits : public CElementTraits<Key>
{
public:
    static ULONG Hash(const Key& element)
    {
        return element.GetHash();
    }

    static bool CompareElements(const Key& element1, const Key& element2)
    {
        return (element1 == element2);
    }
};

class CTextDimsKey
{
private:
    ULONG m_hash;

protected:
    CStringW m_str;
    CAutoPtr<STSStyle> m_style;

public:
    CTextDimsKey(const CStringW& str, STSStyle& style);
    CTextDimsKey(const CTextDimsKey& textDimsKey);

    ULONG GetHash() const { return m_hash; }

    void UpdateHash();

    bool operator==(const CTextDimsKey& textDimsKey) const;
};

class CPolygonPathKey
{
private:
    ULONG m_hash;

protected:
    CStringW m_str;
    double m_scalex, m_scaley;

public:
    CPolygonPathKey(const CStringW& str, double scalex, double scaley);
    CPolygonPathKey(const CPolygonPathKey& polygonPathKey);

    ULONG GetHash() const { return m_hash; }

    void UpdateHash();

    bool operator==(const CPolygonPathKey& polygonPathKey) const;
};
