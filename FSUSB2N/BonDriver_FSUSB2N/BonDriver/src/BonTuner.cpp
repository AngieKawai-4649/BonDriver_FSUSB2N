#include "stdafx.h"
#include "BonTuner.h"

//#include <fstream>

#define ASYNCTSQUEUENUM     24  // Default 3M (128K*24) bytes
#define ASYNCTSQUEUEMAX     240 // Maximum growth 30M (128K*240) bytes
#define ASYNCTSEMPTYBORDER  12  // Empty stock at least 1.5M (128K*12) bytes
#define TSDATASIZE          USBBULK_XFERSIZE // = 128K bytes
#define TSTHREADWAIT        1000
#define SUSPENDTIMEOUT      5000

using namespace std ;

//ofstream debugOut("R:\\FSUSB2N.txt");

#pragma warning( push )
#pragma warning( disable : 4273 )
extern "C" __declspec(dllexport) IBonDriver* CreateBonDriver()
{
    return (CBonTuner::m_pThis) ? CBonTuner::m_pThis : ((IBonDriver*) new CBonTuner);
}
#pragma warning( pop )

// 静的メンバ初期化
CBonTuner * CBonTuner::m_pThis = NULL;
HINSTANCE CBonTuner::m_hModule = NULL;

CBonTuner::CBonTuner(): 
    m_dwCurSpace(0),
    m_dwCurChannel(0),
    usbDev(NULL),
    pDev(NULL),
    m_TsBuffSize(NULL),
    m_iTunerID(-1),
    m_hTsRecv(NULL),
    m_indexTsBuff(0),
    m_pTsBuff(NULL),
    m_dwSetTunerOpenRetryMaxCnt(0),
    m_dwSetTunerOpenRetryWaitTime(0),
    m_SetTunerIdWaitTime(0),
    m_SetTunerStandbyInitWaitTime(0),
    m_SetTunerInitDemoWaitTime(0),
    m_SetDemoEndWaitTime(0),
    m_SetResetDemoWaitTime(0),
    m_SetDemoWaitTime(0),
    m_SetTsResumeWaitTime(0),
    m_hThread(NULL) ,
    m_evThreadSuspend(NULL),
    m_evThreadResume(NULL),
    m_cntThreadSuspend(0),
    m_AsyncTSFifo(NULL),

    m_hDecoderModule(NULL),
    m_CreateInstance(NULL),
    m_b25decoderIF(NULL),
    m_nullPacket(0),
    m_multi2Round(0),
    m_CardResponseTime(0)

{
    m_pThis = this;
}

CBonTuner::~CBonTuner()
{
    // 開かれてる場合は閉じる
    CloseTuner();
    m_pThis = NULL;
}

const BOOL CBonTuner::OpenTuner()
{
    TCHAR strExePath[MAX_PATH] = _T("");
    GetModuleFileName(m_hModule, strExePath, MAX_PATH);

    TCHAR szPath[_MAX_PATH];
    TCHAR szDrive[_MAX_DRIVE];
    TCHAR szDir[_MAX_DIR];
    TCHAR szFname[_MAX_FNAME];
    TCHAR szExt[_MAX_EXT];

    if (IsTunerOpening())return FALSE;

    _tsplitpath_s(strExePath, szDrive, _MAX_DRIVE, szDir, _MAX_DIR, szFname, _MAX_FNAME, szExt, _MAX_EXT);
    _tmakepath_s(szPath, _MAX_PATH, szDrive, szDir, NULL, NULL);

    if (_tcslen(szFname) > _tcslen(_T("BonDriver_FSUSB2N-"))) {
        m_iTunerID = _ttoi(szFname + _tcslen(_T("BonDriver_FSUSB2N-")));
    }

    TCHAR ini_file_path[MAX_PATH];
    _tcscpy_s(ini_file_path, MAX_PATH, szPath);
    _tcsncat_s(ini_file_path, MAX_PATH - _tcslen(ini_file_path), szFname, _MAX_FNAME);
    _tcsncat_s(ini_file_path, MAX_PATH - _tcslen(ini_file_path), _T(".ini"), sizeof(_T(".ini")) / sizeof(TCHAR));

    // iniファイルからパラメータを読み込む
  
    m_dwSetTunerOpenRetryMaxCnt     = GetPrivateProfileInt(_T("PARAMETERS"), _T("SetTunerOpenRetryMaxCnt"),        5,      ini_file_path);
    m_dwSetTunerOpenRetryWaitTime   = GetPrivateProfileInt(_T("PARAMETERS"), _T("SetTunerOpenRetryWaitTime"),      50,     ini_file_path);
    m_SetTunerIdWaitTime            = GetPrivateProfileInt(_T("PARAMETERS"), _T("SetTunerIdWaitTime"),             80,     ini_file_path);
    m_SetTunerStandbyInitWaitTime   = GetPrivateProfileInt(_T("PARAMETERS"), _T("SetTunerStandbyInitWaitTime"),    80,     ini_file_path);
    m_SetTunerInitDemoWaitTime      = GetPrivateProfileInt(_T("PARAMETERS"), _T("SetTunerInitDemoWaitTime"),       80,     ini_file_path);
    m_SetDemoEndWaitTime            = GetPrivateProfileInt(_T("PARAMETERS"), _T("SetDemoEndWaitTime"),             300,    ini_file_path);
    m_SetResetDemoWaitTime          = GetPrivateProfileInt(_T("PARAMETERS"), _T("SetResetDemoWaitTime"),           5,      ini_file_path);
    m_SetDemoWaitTime               = GetPrivateProfileInt(_T("PARAMETERS"), _T("SetDemoWaitTime"),                20,     ini_file_path);
    m_SetTsResumeWaitTime           = GetPrivateProfileInt(_T("PARAMETERS"), _T("SetTsResumeWaitTime"),            10,     ini_file_path);

    m_CardResponseTime = GetPrivateProfileInt(_T("PARAMETERS"), _T("CardResponseTime"), 100, ini_file_path);

    // デコーダーdllをロードする

    TCHAR decoder[MAX_PATH]{};
    GetPrivateProfileString(_T("DECODER"), _T("InternalReader"), NULL, decoder, MAX_PATH, ini_file_path);
    if (_tcslen(decoder) > 0) {
        LPTSTR dec, np, mr;
        dec = decoder;
        np = _tcschr(decoder, _T(','));
        if (np == NULL) {
            return(FALSE);
        }
        *np = _T('\0');
        np++;
        m_nullPacket = static_cast<BYTE>(_tcstoul(np, &mr, 10));
        if (*mr != _T(',')) {
            return(FALSE);
        }
        *mr = _T('\0');
        mr++;
        m_multi2Round = static_cast<BYTE>(_tcstoul(mr, &np, 10));
        if (*np != _T('\0')) {
            return(FALSE);
        }

        m_hDecoderModule = LoadLibrary(dec);
        if (m_hDecoderModule == NULL) {
            return(FALSE);
        }
        m_CreateInstance = (IB25Decoder2 * (*)())::GetProcAddress(m_hDecoderModule, "CreateB25Decoder2");
        if (m_CreateInstance == NULL) {
            FreeLibrary(m_hDecoderModule);
            return(FALSE);
        }
        m_b25decoderIF = m_CreateInstance();
        if (m_b25decoderIF == NULL) {
            FreeLibrary(m_hDecoderModule);
            return(FALSE);
        }
    }

    // iniファイルからチューニングスペース、チャンネル情報を読み込む
    if (!m_chSet.ParseText(ini_file_path)) {
        CloseTuner();
        return FALSE;
    }

    BOOL rtn = FALSE;
    for (DWORD i = 0; i < m_dwSetTunerOpenRetryMaxCnt; i++) {
        EM2874Device* pDevTmp;
        if (pDevTmp = EM2874Device::AllocDevice(m_iTunerID)) {
            // Deviceを確保した
            usbDev = pDevTmp;
            // Device初期化
            usbDev->initDevice2();

            Sleep(m_SetTunerIdWaitTime);

            if (usbDev->getDeviceID() == 2) {
                pDev = new Ktv2Device(usbDev);
            }
            else {
                pDev = new Ktv1Device(usbDev);
            }

            Sleep(m_SetTunerStandbyInitWaitTime);

            pDev->InitTuner();

            Sleep(m_SetTunerInitDemoWaitTime);

            pDev->InitDeMod();
            pDev->ResetDeMod();

            Sleep(m_SetDemoEndWaitTime);

            rtn = TRUE;
            break;
        }
    }
    if (!rtn) {
        CloseTuner();
        return FALSE;
    }


    if ((m_TsBuffSize = (int*)VirtualAlloc(NULL, RINGBUFF_SIZE * USBBULK_XFERSIZE + 0x1000, MEM_COMMIT, PAGE_READWRITE)) != NULL) {
        m_pTsBuff = (BYTE*)(m_TsBuffSize + 0x400);
        m_indexTsBuff = 0;
        m_cntThreadSuspend = 0;

        usbDev->SetBuffer((void*)m_TsBuffSize);
        usbDev->TransferStart();

        m_evThreadSuspend = CreateEvent(NULL, FALSE, FALSE, NULL);
        m_evThreadResume = CreateEvent(NULL, FALSE, FALSE, NULL);
        m_cntThreadSuspend = 0;

        // Initializing asynchronized buffering based contexts.
        m_AsyncTSFifo = new CAsyncFifo(ASYNCTSQUEUENUM, ASYNCTSQUEUEMAX, ASYNCTSEMPTYBORDER, TSDATASIZE, TSTHREADWAIT);

        if ((m_hThread = (HANDLE)_beginthreadex(NULL, 0, TsThread, (PVOID)this, 0, NULL)) != 0) {
            ::SetThreadPriority(m_hThread, THREAD_PRIORITY_HIGHEST/*TIME_CRITICAL*/);
            m_hTsRecv = ::CreateEvent(NULL, FALSE, FALSE, NULL);
        }
        else {
            m_hThread = NULL;
            rtn = FALSE;
        }
    }
    else {
        rtn = FALSE;
    }

    // デバイス使用準備完了 選局はまだ
    if (!rtn) {
        CloseTuner();
        return(FALSE);
    }

    if (m_b25decoderIF != NULL) {
        m_b25decoderIF->usbHandleExport(usbDev->getUsbHandle(), m_CardResponseTime);
        if (rtn = m_b25decoderIF->Initialize()) {
            m_b25decoderIF->DiscardNullPacket(m_nullPacket);
            m_b25decoderIF->EnableEmmProcess(FALSE);
            m_b25decoderIF->SetMulti2Round(m_multi2Round);
            m_b25decoderIF->DiscardScramblePacket(FALSE);
        }
        else {
            CloseTuner();
        }
    }

    return rtn;
}

void CBonTuner::CloseTuner()
{
    if(m_hThread != NULL) {
        if(m_evThreadSuspend) PulseEvent(m_evThreadSuspend) ;
        if(m_evThreadResume) PulseEvent(m_evThreadResume) ;
        usbDev->TransferStop();
        if(::WaitForSingleObject(m_hThread, 5000) != WAIT_OBJECT_0) {
            ::TerminateThread(m_hThread, 0);
        }
        ::CloseHandle(m_hThread);
        ::CloseHandle(m_hTsRecv);
        m_hThread = NULL;
    }

    if(m_TsBuffSize) {
        usbDev->TransferStop();
        usbDev->SetBuffer(NULL);
        ::VirtualFree( (LPVOID)m_TsBuffSize, 0, MEM_RELEASE );
        m_TsBuffSize = NULL;
    }

    if(pDev) {
        delete pDev;
        pDev = NULL;
    }

    if (m_b25decoderIF != NULL) {
        m_b25decoderIF->Release();
        m_b25decoderIF = NULL;
    }
    if (m_hDecoderModule) {
        FreeLibrary(m_hDecoderModule);
        m_hDecoderModule = NULL;
    }
 
    if(usbDev) {
        delete usbDev;
        usbDev = NULL;
    }

    if(m_AsyncTSFifo) {
      delete m_AsyncTSFifo ;
      m_AsyncTSFifo = NULL ;
    }
    if(m_evThreadSuspend) {
      ::CloseHandle(m_evThreadSuspend) ;
      m_evThreadSuspend=NULL ;
    }
    if(m_evThreadResume) {
      ::CloseHandle(m_evThreadResume) ;
      m_evThreadResume=NULL ;
    }
}


const BOOL CBonTuner::SetChannel(const BYTE bCh)
{
    // IBonDriverとの互換性を保つために暫定
    return(TRUE);
}

const float CBonTuner::GetSignalLevel(void)
{
    if(pDev == NULL) return 0.0f;
    return pDev->DeMod_GetQuality() * 0.01f;
}

const DWORD CBonTuner::WaitTsStream(const DWORD dwTimeOut)
{
    if(GetReadyCount() > 0) {
        return WAIT_OBJECT_0;
    }
    if(m_TsBuffSize == NULL) {
        ::Sleep(dwTimeOut < 2000 ? dwTimeOut : 2000 );
        return WAIT_TIMEOUT;
    }

    return ::WaitForSingleObject(m_hTsRecv, dwTimeOut);
}

const DWORD CBonTuner::GetReadyCount()
{
    // 取り出し可能TSデータ数を取得する
    return (DWORD)m_AsyncTSFifo->Size();
}

const BOOL CBonTuner::GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain)
{
    BYTE *pSrc = NULL;

    // TSデータをBufferから取り出す
    if(GetTsStream(&pSrc, pdwSize, pdwRemain)){
        if(*pdwSize) {
            ::CopyMemory(pDst, pSrc, *pdwSize);
        }
        return TRUE;
    }

    return FALSE;
}

const BOOL CBonTuner::GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain)
{
 
    m_AsyncTSFifo->Pop(ppDst,pdwSize,pdwRemain) ;

    if (m_b25decoderIF != NULL && *pdwSize > 0) {
        m_b25decoderIF->Decode(*ppDst, *pdwSize, ppDst, pdwSize);
    }

    return TRUE;
}

void CBonTuner::PurgeTsStream()
{
    // TsThreadが一時停止するのを待機する
    if(m_hThread) {
      ThreadSuspend() ;
      if(m_hTsRecv) {
        ResetEvent(m_hTsRecv) ;
      }
    }
    // バッファから取り出し可能データをパージする
    m_AsyncTSFifo->Purge() ;

    if(m_TsBuffSize && m_TsBuffSize[m_indexTsBuff] >= 0) {
     // 現在のカーソル位置から次回バッファリングされる位置を走査
      const int indexCurrent = m_indexTsBuff ;
      do {
        if(m_TsBuffSize[m_indexTsBuff] == -1) break ;
        if(m_TsBuffSize[++m_indexTsBuff] <= -2) m_indexTsBuff=0 ;
      }while(indexCurrent!=m_indexTsBuff) ;
    }

    // TsThreadを再開する
    if(m_hThread) {
      ThreadResume();
    }
}

void CBonTuner::Release()
{
    // インスタンス開放
    delete this;
}

LPCTSTR CBonTuner::GetTunerName(void)
{
    // チューナ名を返す
    return _T("KTV-FSUSB2N");
}

const BOOL CBonTuner::IsTunerOpening(void)
{
    return pDev ? TRUE : FALSE;
}

LPCTSTR CBonTuner::EnumTuningSpace(const DWORD dwSpace)
{
    vector<SPACE_DATA>::iterator itr = find_if(m_chSet.spaceVec.begin(), m_chSet.spaceVec.end(), [dwSpace](const SPACE_DATA& s) {return(s.cSpace == dwSpace); });
    return((itr == m_chSet.spaceVec.end()) ? NULL : itr->tszName.c_str());
}

LPCTSTR CBonTuner::EnumChannelName(const DWORD dwSpace, const DWORD dwChannel)
{
    vector<SPACE_DATA>::iterator itr = find_if(m_chSet.spaceVec.begin(), m_chSet.spaceVec.end(), [dwSpace](const SPACE_DATA& s) {return(s.cSpace == dwSpace); });
    if (itr == m_chSet.spaceVec.end()) {
        return NULL;
    }
    else {
        return((itr->chVec.size() <= dwChannel) ? NULL : itr->chVec[dwChannel].tszName.c_str());
    }
}

const BOOL CBonTuner::SetChannel(const DWORD dwSpace, const DWORD dwChannel)
{
    if(pDev == NULL) return FALSE;
    vector<SPACE_DATA>::iterator itr = find_if(m_chSet.spaceVec.begin(), m_chSet.spaceVec.end(), [dwSpace](const SPACE_DATA& s) {return(s.cSpace == dwSpace); });
    if (itr == m_chSet.spaceVec.end()) {
        return FALSE;
    }
    if (itr->chVec.size() <= dwChannel) {
        return FALSE;
    }

 

    // Channel変更
    ThreadSuspend() ;

    usbDev->TransferPause();

    pDev->SetFrequency(itr->chVec[dwChannel].ulFreq);
    ::Sleep(m_SetResetDemoWaitTime);
    pDev->ResetDeMod();
    ::Sleep(m_SetDemoWaitTime);

    usbDev->TransferResume();
    ::Sleep(m_SetTsResumeWaitTime);

    PurgeTsStream();

    Sleep(200);

    if (m_b25decoderIF) {
        m_b25decoderIF->Reset();
    }

    Sleep(200);

    ThreadResume() ;



    // Channel情報更新
    m_dwCurSpace = dwSpace;
    m_dwCurChannel = dwChannel;

    return TRUE;
}

const DWORD CBonTuner::GetCurSpace(void)
{
    // 現在のチューニング空間を返す
    return m_dwCurSpace;
}

const DWORD CBonTuner::GetCurChannel(void)
{
    // 現在のChannelを返す
    return m_dwCurChannel;
}

unsigned int CBonTuner::TsThreadMain ()
{
    const unsigned int BuffBlockSize = -m_TsBuffSize[0x3ff];
    DWORD dwRet;
    int nRet;

    if(!usbDev)
      return 0;

    for(;;)
    {
        HANDLE evTs = usbDev->GetHandle() ;  if(!evTs) break ;
        dwRet = ::WaitForSingleObject( evTs , TSTHREADWAIT );

        {
          exclusive_lock lock(&m_exSuspend) ;
          if(m_cntThreadSuspend>0) {
              lock.unlock() ;
              ::SetEvent(m_evThreadSuspend) ;
              ::WaitForSingleObject(m_evThreadResume,SUSPENDTIMEOUT) ;
              continue ;
          }
        }
        if( dwRet == WAIT_FAILED ) {
            break;
        }else if( dwRet == WAIT_OBJECT_0 /*|| dwRet == WAIT_TIMEOUT*/ )
        {
          BOOL nextReadIn ;
          do {
            nRet = usbDev->DispatchTSRead(&nextReadIn);
            if(nRet > 0)  {
              size_t n=0;
              if(m_TsBuffSize != NULL) {
                const int indexCurrent = m_indexTsBuff;
                int ln;
                do {
                  ln = m_TsBuffSize[m_indexTsBuff];
                  if(ln>=0) {
                    // Copying received buffers to fifo.
                    BYTE *p =  m_pTsBuff + (m_indexTsBuff * BuffBlockSize) ;
                    n+=m_AsyncTSFifo->Push(p,ln) ;
                    m_indexTsBuff++ ;
                  }else if(ln <= -2) {
                    m_indexTsBuff = 0 ;
                  }
                  if(m_cntThreadSuspend) break ;
                } while(ln!=-1 && m_indexTsBuff != indexCurrent);
              }
              if(n>0) ::SetEvent(m_hTsRecv);
            }
            if(nRet < 0)  break;
            if(m_cntThreadSuspend) break ;
          }while(nextReadIn) ;
          if(nRet < 0)    break;
        }
    }
    return(0);
}
unsigned int __stdcall CBonTuner::TsThread (PVOID pv)
{
    register CBonTuner *_this = static_cast<CBonTuner*>(pv) ;
    unsigned int r = _this->TsThreadMain () ;
    ::_endthreadex (r);
    return r;
}
void CBonTuner::ThreadSuspend()
{
  exclusive_lock lock(&m_exSuspend) ;
  if(!m_cntThreadSuspend++) {
      lock.unlock() ;
      if(usbDev) {
        HANDLE evTs = usbDev->GetHandle() ;
        if(evTs) ::SetEvent(evTs) ;
      }
      if(m_evThreadSuspend) ::WaitForSingleObject(m_evThreadSuspend,SUSPENDTIMEOUT);
  }
}
void CBonTuner::ThreadResume()
{
  exclusive_lock lock(&m_exSuspend) ;
  if(!--m_cntThreadSuspend) {
      lock.unlock() ;
      if(m_evThreadResume) ::SetEvent(m_evThreadResume) ;
  }
}
