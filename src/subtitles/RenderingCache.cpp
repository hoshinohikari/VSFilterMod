/*
 * Rendering cache utilities (ported from MPC-HC)
 */

#include "stdafx.h"
#include "RenderingCache.h"

CTextDimsKey::CTextDimsKey(const CStringW& str, STSStyle& style)
    : m_str(str)
    , m_style(DEBUG_NEW STSStyle(style))
{
    UpdateHash();
}

CTextDimsKey::CTextDimsKey(const CTextDimsKey& textDimsKey)
    : m_hash(textDimsKey.m_hash)
    , m_str(textDimsKey.m_str)
    , m_style(DEBUG_NEW STSStyle(*textDimsKey.m_style))
{
}

void CTextDimsKey::UpdateHash()
{
    m_hash  = CStringElementTraits<CStringW>::Hash(m_str);
    m_hash += m_hash << 5;
    m_hash += m_style->charSet;
    m_hash += m_hash << 5;
    m_hash += CStringElementTraits<CString>::Hash(m_style->fontName);
    m_hash += m_hash << 5;
    m_hash += int(m_style->fontSize);
    m_hash += m_hash << 5;
    m_hash += int(m_style->fontSpacing);
    m_hash += m_hash << 5;
    m_hash += m_style->fontWeight;
    m_hash += m_hash << 5;
    m_hash += m_style->fItalic;
    m_hash += m_hash << 5;
    m_hash += m_style->fUnderline;
    m_hash += m_hash << 5;
    m_hash += m_style->fStrikeOut;
#ifdef _VSMOD
    m_hash += m_hash << 5;
    m_hash += int(m_style->mod_fontOrient);
#endif
}

bool CTextDimsKey::operator==(const CTextDimsKey& textDimsKey) const
{
    return m_str == textDimsKey.m_str
           && m_style->charSet == textDimsKey.m_style->charSet
           && m_style->fontName == textDimsKey.m_style->fontName
           && IsNearlyEqual(m_style->fontSize, textDimsKey.m_style->fontSize, 1e-6)
           && IsNearlyEqual(m_style->fontSpacing, textDimsKey.m_style->fontSpacing, 1e-6)
           && m_style->fontWeight == textDimsKey.m_style->fontWeight
           && m_style->fItalic == textDimsKey.m_style->fItalic
           && m_style->fUnderline == textDimsKey.m_style->fUnderline
           && m_style->fStrikeOut == textDimsKey.m_style->fStrikeOut
#ifdef _VSMOD
           && m_style->mod_fontOrient == textDimsKey.m_style->mod_fontOrient
#endif
           ;
}

CPolygonPathKey::CPolygonPathKey(const CStringW& str, double scalex, double scaley)
    : m_str(str)
    , m_scalex(scalex)
    , m_scaley(scaley)
{
    UpdateHash();
}

CPolygonPathKey::CPolygonPathKey(const CPolygonPathKey& polygonPathKey)
    : m_hash(polygonPathKey.m_hash)
    , m_str(polygonPathKey.m_str)
    , m_scalex(polygonPathKey.m_scalex)
    , m_scaley(polygonPathKey.m_scaley)
{
}

void CPolygonPathKey::UpdateHash()
{
    m_hash = CStringElementTraits<CStringW>::Hash(m_str);
    m_hash += m_hash << 5;
    m_hash += int(m_scalex * 1e6);
    m_hash += m_hash << 5;
    m_hash += int(m_scaley * 1e6);
}

bool CPolygonPathKey::operator==(const CPolygonPathKey& polygonPathKey) const
{
    return m_str == polygonPathKey.m_str
           && IsNearlyEqual(m_scalex, polygonPathKey.m_scalex, 1e-6)
           && IsNearlyEqual(m_scaley, polygonPathKey.m_scaley, 1e-6);
}
