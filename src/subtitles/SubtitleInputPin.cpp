/*
 *	Copyright (C) 2003-2006 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "SubtitleInputPin.h"
#include "VobSubFile.h"
#include "RTS.h"
#include "SSF.h"
#include "RenderedHdmvSubtitle.h"
#include "STS.h"

#include <initguid.h>
#include <moreuuids.h>

// our first format id
#define __GAB1__ "GAB1"

// our tags for __GAB1__ (ushort) + size (ushort)

// "lang" + '0'
#define __GAB1_LANGUAGE__ 0
// (int)start+(int)stop+(char*)line+'0'
#define __GAB1_ENTRY__ 1
// L"lang" + '0'
#define __GAB1_LANGUAGE_UNICODE__ 2
// (int)start+(int)stop+(WCHAR*)line+'0'
#define __GAB1_ENTRY_UNICODE__ 3

// same as __GAB1__, but the size is (uint) and only __GAB1_LANGUAGE_UNICODE__ is valid
#define __GAB2__ "GAB2"

// (BYTE*)
#define __GAB1_RAWTEXTSUBTITLE__ 4

CSubtitleInputPin::CSubtitleInputPin(CBaseFilter* pFilter, CCritSec* pLock, CCritSec* pSubLock, HRESULT* phr)
    : CBaseInputPin(NAME("CSubtitleInputPin"), pFilter, pLock, phr, L"Input")
    , m_pSubLock(pSubLock)
{
    m_bCanReconnectWhenActive = TRUE;
    m_decodeThread = std::thread([this]() {
        DecodeSamples();
    });
}

CSubtitleInputPin::~CSubtitleInputPin()
{
    m_bExitDecodingThread = m_bStopDecoding = true;
    m_condQueueReady.notify_one();
    if(m_decodeThread.joinable())
    {
        m_decodeThread.join();
    }
}

HRESULT CSubtitleInputPin::CheckMediaType(const CMediaType* pmt)
{
    return pmt->majortype == MEDIATYPE_Text && (pmt->subtype == MEDIASUBTYPE_NULL || pmt->subtype == FOURCCMap((DWORD)0))
           || pmt->majortype == MEDIATYPE_Subtitle && (pmt->subtype == MEDIASUBTYPE_UTF8 || pmt->subtype == MEDIASUBTYPE_WEBVTT)
           || pmt->majortype == MEDIATYPE_Subtitle && (pmt->subtype == MEDIASUBTYPE_SSA || pmt->subtype == MEDIASUBTYPE_ASS || pmt->subtype == MEDIASUBTYPE_ASS2)
           || pmt->majortype == MEDIATYPE_Subtitle && pmt->subtype == MEDIASUBTYPE_SSF
           || pmt->majortype == MEDIATYPE_Subtitle && (pmt->subtype == MEDIASUBTYPE_VOBSUB)
           || pmt->majortype == MEDIATYPE_Subtitle && pmt->subtype == MEDIASUBTYPE_HDMVSUB
           || pmt->majortype == MEDIATYPE_Subtitle && pmt->subtype == MEDIASUBTYPE_DVB_SUBTITLES
           ? S_OK
           : E_FAIL;
}

HRESULT CSubtitleInputPin::CompleteConnect(IPin* pReceivePin)
{
    InvalidateSamples();

    if(m_mt.majortype == MEDIATYPE_Text)
    {
        if(!(m_pSubStream = DNew CRenderedTextSubtitle(m_pSubLock))) return E_FAIL;
        CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;
        pRTS->m_name = CString(GetPinName(pReceivePin)) + _T(" (embeded)");
        pRTS->m_dstScreenSize = CSize(384, 288);
        pRTS->CreateDefaultStyle(DEFAULT_CHARSET);
    }
    else if(m_mt.majortype == MEDIATYPE_Subtitle)
    {
        SUBTITLEINFO*	psi		= (SUBTITLEINFO*)m_mt.pbFormat;
        DWORD			dwOffset	= 0;
        CString			name;
        LCID			lcid = 0;

        if(psi != NULL)
        {
            dwOffset = psi->dwOffset;

            name = ISO6392ToLanguage(psi->IsoLang);
            lcid = ISO6392ToLcid(psi->IsoLang);
            if(name.IsEmpty()) name = _T("Unknown");
            if(wcslen(psi->TrackName) > 0) name += _T(" (") + CString(psi->TrackName) + _T(")");
        }

          if(m_mt.subtype == MEDIASUBTYPE_UTF8
              || m_mt.subtype == MEDIASUBTYPE_WEBVTT
           /*|| m_mt.subtype == MEDIASUBTYPE_USF*/
           || m_mt.subtype == MEDIASUBTYPE_SSA
           || m_mt.subtype == MEDIASUBTYPE_ASS
           || m_mt.subtype == MEDIASUBTYPE_ASS2)
        {
            if(!(m_pSubStream = DNew CRenderedTextSubtitle(m_pSubLock))) return E_FAIL;
            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;
            pRTS->m_name = name;
            pRTS->m_lcid = lcid;
            pRTS->m_dstScreenSize = CSize(384, 288);
            pRTS->CreateDefaultStyle(DEFAULT_CHARSET);

            if(dwOffset > 0 && m_mt.cbFormat - dwOffset > 0)
            {
                CMediaType mt = m_mt;
                if(mt.pbFormat[dwOffset+0] != 0xef
                   && mt.pbFormat[dwOffset+1] != 0xbb
                   && mt.pbFormat[dwOffset+2] != 0xfb)
                {
                    dwOffset -= 3;
                    mt.pbFormat[dwOffset+0] = 0xef;
                    mt.pbFormat[dwOffset+1] = 0xbb;
                    mt.pbFormat[dwOffset+2] = 0xbf;
                }

                pRTS->Open(mt.pbFormat + dwOffset, mt.cbFormat - dwOffset, DEFAULT_CHARSET, pRTS->m_name);
            }

        }
        else if(m_mt.subtype == MEDIASUBTYPE_SSF)
        {
            if(!(m_pSubStream = DNew ssf::CRenderer(m_pSubLock))) return E_FAIL;
            ssf::CRenderer* pSSF = (ssf::CRenderer*)(ISubStream*)m_pSubStream;
            pSSF->Open(ssf::MemoryInputStream(m_mt.pbFormat + dwOffset, m_mt.cbFormat - dwOffset, false, false), name);
        }
        else if(m_mt.subtype == MEDIASUBTYPE_VOBSUB)
        {
            if(!(m_pSubStream = DNew CVobSubStream(m_pSubLock))) return E_FAIL;
            CVobSubStream* pVSS = (CVobSubStream*)(ISubStream*)m_pSubStream;
            pVSS->Open(name, m_mt.pbFormat + dwOffset, m_mt.cbFormat - dwOffset);
        }
        else if(m_mt.subtype == MEDIASUBTYPE_HDMVSUB)
        {
            if(!(m_pSubStream = DNew CRenderedHdmvSubtitle(m_pSubLock, ST_HDMV))) return E_FAIL;
        }
        else if(m_mt.subtype == MEDIASUBTYPE_DVB_SUBTITLES)
        {
            if(!(m_pSubStream = DNew CRenderedHdmvSubtitle(m_pSubLock, ST_DVB))) return E_FAIL;
        }
    }

    AddSubStream(m_pSubStream);

    return __super::CompleteConnect(pReceivePin);
}

HRESULT CSubtitleInputPin::BreakConnect()
{
    InvalidateSamples();

    RemoveSubStream(m_pSubStream);
    m_pSubStream = NULL;

    ASSERT(IsStopped());

    return __super::BreakConnect();
}

STDMETHODIMP CSubtitleInputPin::ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt)
{
    if(m_Connected)
    {
        InvalidateSamples();

        RemoveSubStream(m_pSubStream);
        m_pSubStream = NULL;

        m_Connected->Release();
        m_Connected = NULL;
    }

    return __super::ReceiveConnection(pConnector, pmt);
}

STDMETHODIMP CSubtitleInputPin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    CAutoLock cAutoLock(&m_csReceive);

    InvalidateSamples();

    if(m_mt.majortype == MEDIATYPE_Text
       || m_mt.majortype == MEDIATYPE_Subtitle
       && (m_mt.subtype == MEDIASUBTYPE_UTF8
           || m_mt.subtype == MEDIASUBTYPE_WEBVTT
           /*|| m_mt.subtype == MEDIASUBTYPE_USF*/
           || m_mt.subtype == MEDIASUBTYPE_SSA
           || m_mt.subtype == MEDIASUBTYPE_ASS
           || m_mt.subtype == MEDIASUBTYPE_ASS2))
    {
        CAutoLock cAutoLock(m_pSubLock);
        CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;
        pRTS->RemoveAll();
        pRTS->CreateSegments();
    }
    else if(m_mt.majortype == MEDIATYPE_Subtitle && m_mt.subtype == MEDIASUBTYPE_SSF)
    {
        CAutoLock cAutoLock(m_pSubLock);
        ssf::CRenderer* pSSF = (ssf::CRenderer*)(ISubStream*)m_pSubStream;
        // LAME, implement RemoveSubtitles
        DWORD dwOffset = ((SUBTITLEINFO*)m_mt.pbFormat)->dwOffset;
        pSSF->Open(ssf::MemoryInputStream(m_mt.pbFormat + dwOffset, m_mt.cbFormat - dwOffset, false, false), _T(""));
        // pSSF->RemoveSubtitles();
    }
    else if(m_mt.majortype == MEDIATYPE_Subtitle && (m_mt.subtype == MEDIASUBTYPE_VOBSUB))
    {
        CAutoLock cAutoLock(m_pSubLock);
        CVobSubStream* pVSS = (CVobSubStream*)(ISubStream*)m_pSubStream;
        pVSS->RemoveAll();
    }
    else if(m_mt.majortype == MEDIATYPE_Subtitle && (m_mt.subtype == MEDIASUBTYPE_HDMVSUB || m_mt.subtype == MEDIASUBTYPE_DVB_SUBTITLES))
    {
        CAutoLock cAutoLock(m_pSubLock);
        CRenderedHdmvSubtitle* pHdmvSubtitle = (CRenderedHdmvSubtitle*)(ISubStream*)m_pSubStream;
        pHdmvSubtitle->NewSegment(tStart, tStop, dRate);
    }

    return __super::NewSegment(tStart, tStop, dRate);
}

[uuid("D3D92BC3-713B-451B-9122-320095D51EA5")]
interface IMpeg2DemultiplexerTesting :
public IUnknown
{
    STDMETHOD(GetMpeg2StreamType)(ULONG* plType) = NULL;
    STDMETHOD(toto)() = NULL;
};


STDMETHODIMP CSubtitleInputPin::Receive(IMediaSample* pSample)
{
    HRESULT hr = __super::Receive(pSample);
    if(FAILED(hr)) return hr;

    CAutoLock cAutoLock(&m_csReceive);

    REFERENCE_TIME tStart, tStop;
    hr = pSample->GetTime(&tStart, &tStop);

    switch(hr)
    {
    case S_OK:
        tStart += m_tStart;
        tStop += m_tStart;
        break;
    case VFW_S_NO_STOP_TIME:
        tStart += m_tStart;
        tStop = INVALID_TIME;
        break;
    case VFW_E_SAMPLE_TIME_NOT_SET:
        tStart = tStop = INVALID_TIME;
        break;
    default:
        ASSERT(FALSE);
        return hr;
    }

    const bool useMediaSample = (m_mt.subtype == MEDIASUBTYPE_HDMVSUB || m_mt.subtype == MEDIASUBTYPE_DVB_SUBTITLES);

    if((tStart == INVALID_TIME || tStop == INVALID_TIME) && !useMediaSample)
    {
        ASSERT(FALSE);
        return S_OK;
    }

    if(useMediaSample)
    {
        std::unique_lock<std::mutex> lock(m_mutexQueue);
        m_sampleQueue.emplace_back(std::make_unique<SubtitleSample>(tStart, tStop, pSample));
        lock.unlock();
        m_condQueueReady.notify_one();
        return S_OK;
    }

    BYTE* pData = NULL;
    hr = pSample->GetPointer(&pData);
    if(FAILED(hr) || pData == NULL) return hr;

    int len = pSample->GetActualDataLength();
    if(len <= 0) return S_OK;

    {
        std::unique_lock<std::mutex> lock(m_mutexQueue);
        m_sampleQueue.emplace_back(std::make_unique<SubtitleSample>(tStart, tStop, pData, size_t(len)));
        lock.unlock();
        m_condQueueReady.notify_one();
    }

    return S_OK;
}

STDMETHODIMP CSubtitleInputPin::EndOfStream(void)
{
    HRESULT hr = __super::EndOfStream();

    if(SUCCEEDED(hr))
    {
        std::unique_lock<std::mutex> lock(m_mutexQueue);
        m_sampleQueue.emplace_back(nullptr); // end-of-stream marker
        lock.unlock();
        m_condQueueReady.notify_one();
    }

    return hr;
}

bool CSubtitleInputPin::IsRLECodedSub(const CMediaType* pmt) const
{
    return !!(pmt->majortype == MEDIATYPE_Subtitle
              && (pmt->subtype == MEDIASUBTYPE_HDMVSUB
                  || pmt->subtype == MEDIASUBTYPE_DVB_SUBTITLES));
}

void CSubtitleInputPin::DecodeSamples()
{
    for(; !m_bExitDecodingThread;)
    {
        std::unique_lock<std::mutex> lock(m_mutexQueue);

        auto needStopProcessing = [this]() {
            return m_bStopDecoding || m_bExitDecodingThread;
        };

        auto isQueueReady = [&]() {
            return !m_sampleQueue.empty() || needStopProcessing();
        };

        m_condQueueReady.wait(lock, isQueueReady);
        lock.unlock();

        REFERENCE_TIME rtInvalidate = -1;

        if(!needStopProcessing())
        {
            CAutoLock cAutoLock(m_pSubLock);
            lock.lock();

            while(!m_sampleQueue.empty() && !needStopProcessing())
            {
                const auto& pSample = m_sampleQueue.front();

                if(pSample)
                {
                    REFERENCE_TIME rtSampleInvalidate = DecodeSample(pSample);
                    if(rtSampleInvalidate >= 0 && (rtSampleInvalidate < rtInvalidate || rtInvalidate < 0))
                    {
                        rtInvalidate = rtSampleInvalidate;
                    }
                }

                m_sampleQueue.pop_front();
            }
        }

        if(rtInvalidate >= 0)
        {
            // IMPORTANT: m_pSubLock must not be locked when calling this
            InvalidateSubtitle(rtInvalidate, m_pSubStream);
        }
    }
}

REFERENCE_TIME CSubtitleInputPin::DecodeSample(const std::unique_ptr<SubtitleSample>& pSample)
{
    bool fInvalidate = false;

    if(pSample->mediaSample)
    {
        if(m_mt.subtype == MEDIASUBTYPE_HDMVSUB || m_mt.subtype == MEDIASUBTYPE_DVB_SUBTITLES)
        {
            CRenderedHdmvSubtitle* pHdmvSubtitle = (CRenderedHdmvSubtitle*)(ISubStream*)m_pSubStream;
            pHdmvSubtitle->ParseSample(pSample->mediaSample);
        }

        return -1;
    }

    if(pSample->data.size() <= 0)
    {
        return -1;
    }

    BYTE* pData = pSample->data.data();
    int len = (int)pSample->data.size();
    REFERENCE_TIME tStart = pSample->rtStart;
    REFERENCE_TIME tStop = pSample->rtStop;

    if(m_mt.majortype == MEDIATYPE_Text)
    {
        CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;

        if(!strncmp((char*)pData, __GAB1__, strlen(__GAB1__)))
        {
            char* ptr = (char*)&pData[strlen(__GAB1__)+1];
            char* end = (char*)&pData[len];

            while(ptr < end)
            {
                WORD tag = *((WORD*)(ptr));
                ptr += 2;
                WORD size = *((WORD*)(ptr));
                ptr += 2;

                if(tag == __GAB1_LANGUAGE__)
                {
                    pRTS->m_name = CString(ptr);
                }
                else if(tag == __GAB1_ENTRY__)
                {
                    pRTS->Add(AToW(&ptr[8]), false, *(int*)ptr, *(int*)(ptr + 4));
                    fInvalidate = true;
                }
                else if(tag == __GAB1_LANGUAGE_UNICODE__)
                {
                    pRTS->m_name = (WCHAR*)ptr;
                }
                else if(tag == __GAB1_ENTRY_UNICODE__)
                {
                    pRTS->Add((WCHAR*)(ptr + 8), true, *(int*)ptr, *(int*)(ptr + 4));
                    fInvalidate = true;
                }

                ptr += size;
            }
        }
        else if(!strncmp((char*)pData, __GAB2__, strlen(__GAB2__)))
        {
            char* ptr = (char*)&pData[strlen(__GAB2__)+1];
            char* end = (char*)&pData[len];

            while(ptr < end)
            {
                WORD tag = *((WORD*)(ptr));
                ptr += 2;
                DWORD size = *((DWORD*)(ptr));
                ptr += 4;

                if(tag == __GAB1_LANGUAGE_UNICODE__)
                {
                    pRTS->m_name = (WCHAR*)ptr;
                }
                else if(tag == __GAB1_RAWTEXTSUBTITLE__)
                {
                    pRTS->Open((BYTE*)ptr, size, DEFAULT_CHARSET, pRTS->m_name);
                    fInvalidate = true;
                }

                ptr += size;
            }
        }
        else if(pData != 0 && len > 1 && *pData != 0)
        {
            CStringA str((char*)pData, len);

            str.Replace("\r\n", "\n");
            str.Trim();

            if(!str.IsEmpty())
            {
                pRTS->Add(AToW(str), false, (int)(tStart / 10000), (int)(tStop / 10000));
                fInvalidate = true;
            }
        }
    }
    else if(m_mt.majortype == MEDIATYPE_Subtitle)
    {
        if(m_mt.subtype == MEDIASUBTYPE_UTF8 || m_mt.subtype == MEDIASUBTYPE_WEBVTT)
        {
            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;

            CStringW str = UTF8To16(CStringA((LPCSTR)pData, len)).Trim();
            if(!str.IsEmpty())
            {
                if(m_mt.subtype == MEDIASUBTYPE_WEBVTT)
                {
                    WebVTT2SSA(str);
                }
                pRTS->Add(str, true, (int)(tStart / 10000), (int)(tStop / 10000));
                fInvalidate = true;
            }
        }
        else if(m_mt.subtype == MEDIASUBTYPE_SSA || m_mt.subtype == MEDIASUBTYPE_ASS || m_mt.subtype == MEDIASUBTYPE_ASS2)
        {
            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;

            CStringW str = UTF8To16(CStringA((LPCSTR)pData, len)).Trim();
            if(!str.IsEmpty())
            {
                STSEntry stse;

                int fields = m_mt.subtype == MEDIASUBTYPE_ASS2 ? 10 : 9;

                CAtlList<CStringW> sl;
                Explode(str, sl, ',', fields);
                if(sl.GetCount() == fields)
                {
                    stse.readorder = wcstol(sl.RemoveHead(), NULL, 10);
                    stse.layer = wcstol(sl.RemoveHead(), NULL, 10);
                    stse.style = sl.RemoveHead();
                    stse.actor = sl.RemoveHead();
                    stse.marginRect.left = wcstol(sl.RemoveHead(), NULL, 10);
                    stse.marginRect.right = wcstol(sl.RemoveHead(), NULL, 10);
                    stse.marginRect.top = stse.marginRect.bottom = wcstol(sl.RemoveHead(), NULL, 10);
                    if(fields == 10) stse.marginRect.bottom = wcstol(sl.RemoveHead(), NULL, 10);
                    stse.effect = sl.RemoveHead();
                    stse.str = sl.RemoveHead();
                }

                if(!stse.str.IsEmpty())
                {
                    pRTS->Add(stse.str, true, (int)(tStart / 10000), (int)(tStop / 10000),
                              stse.style, stse.actor, stse.effect, stse.marginRect, stse.layer, stse.readorder);
                    fInvalidate = true;
                }
            }
        }
        else if(m_mt.subtype == MEDIASUBTYPE_SSF)
        {
            ssf::CRenderer* pSSF = (ssf::CRenderer*)(ISubStream*)m_pSubStream;

            CStringW str = UTF8To16(CStringA((LPCSTR)pData, len)).Trim();
            if(!str.IsEmpty())
            {
                pSSF->Append(tStart, tStop, str);
                fInvalidate = true;
            }
        }
        else if(m_mt.subtype == MEDIASUBTYPE_VOBSUB)
        {
            CVobSubStream* pVSS = (CVobSubStream*)(ISubStream*)m_pSubStream;
            pVSS->Add(tStart, tStop, pData, len);
        }
    }

    return fInvalidate ? tStart : -1;
}

void CSubtitleInputPin::InvalidateSamples()
{
    m_bStopDecoding = true;
    {
        std::lock_guard<std::mutex> lock(m_mutexQueue);
        m_sampleQueue.clear();
        m_bStopDecoding = false;
    }
}


