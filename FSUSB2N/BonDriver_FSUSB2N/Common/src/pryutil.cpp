//===========================================================================
#include "stdafx.h"
#include <process.h>

#include <locale.h>

#include "pryutil.h"
//---------------------------------------------------------------------------
using namespace std ;

//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------

//===========================================================================
// Functions
//---------------------------------------------------------------------------
ULONGLONG PastSleep(ULONGLONG wait,ULONGLONG start)
{
  if(!wait) return start ;
  ULONGLONG tick = GetTickCount64() ;
  ULONGLONG past = (tick>=start) ? tick-start : MAXULONGLONG-start+1+tick ;
  //if(wait>past) Sleep(wait-past) ;
  if (wait > past) Sleep( static_cast<DWORD>(wait - past));
  return start+wait ;
}
//---------------------------------------------------------------------------
wstring mbcs2wcs(string src)
{
    if(src.empty()) return wstring(L"") ;
    BUFFER<wchar_t> wcs(src.length()*3 + 3);
    size_t wLen = 0;
    setlocale(LC_ALL,"japanese");
    mbstowcs_s(&wLen, wcs.top(), src.length()*3+1 , src.c_str(), _TRUNCATE);
    wstring result = wcs.top() ;
    return result ;
}
//---------------------------------------------------------------------------
string wcs2mbcs(wstring src)
{
    if(src.empty()) return string("") ;
    BUFFER<char> mbcs(src.length()*3 + 3) ;
    size_t mbLen = 0 ;
    setlocale(LC_ALL,"japanese");
    wcstombs_s(&mbLen, mbcs.top(), src.length()*3+1 , src.c_str(), _TRUNCATE);
    string result = mbcs.top() ;
    return result ;
}
//---------------------------------------------------------------------------
string itos(int val,int radix)
{
  BUFFER<char> str(72) ;
  if(!_itoa_s(val,str.top(),70,radix))
    return static_cast<string>(str.top()) ;
  return "NAN" ;
}
//---------------------------------------------------------------------------
wstring itows(int val,int radix)
{
  BUFFER<wchar_t> str(72) ;
  if(!_itow_s(val,str.top(),70,radix))
    return static_cast<wstring>(str.top()) ;
  return L"NAN" ;
}
//---------------------------------------------------------------------------
string upper_case(string str)
{
  BUFFER<char> temp(str.length()+1) ;
  CopyMemory(temp.top(),str.c_str(),(str.length()+1)*sizeof(char)) ;
  _strupr_s(temp.top(),str.length()+1) ;
  return static_cast<string>(temp.top()) ;
}
//---------------------------------------------------------------------------
string lower_case(string str)
{
  BUFFER<char> temp(str.length()+1) ;
  CopyMemory(temp.top(),str.c_str(),(str.length()+1)*sizeof(char)) ;
  _strlwr_s(temp.top(),str.length()+1) ;
  return static_cast<string>(temp.top()) ;
}
//---------------------------------------------------------------------------
string file_path_of(string filename)
{
  char szDrive[_MAX_FNAME] ;
  char szDir[_MAX_FNAME] ;
  _splitpath_s( filename.c_str(), szDrive, _MAX_FNAME, szDir, _MAX_FNAME
    , NULL, 0, NULL, 0 ) ;
  return string(szDrive)+string(szDir) ;
}
//---------------------------------------------------------------------------
string file_prefix_of(string filename)
{
  char szName[_MAX_FNAME] ;
  _splitpath_s( filename.c_str(), NULL, 0, NULL, 0
    , szName, _MAX_FNAME, NULL, 0 ) ;
  return string(szName) ;
}
//===========================================================================
// event_object
//---------------------------------------------------------------------------
static int event_create_count = 0 ;
//---------------------------------------------------------------------------
event_object::event_object(BOOL _InitialState,tstring _name)
{
  if(_name.empty()) _name = _T("event")+titos(event_create_count++) ;
  name = _name ;
  event = CreateEvent(NULL,FALSE,_InitialState,name.c_str()) ;
#ifdef _DEBUG
  if(event) {
    TRACE(L"event_object created. [name=%s]\r\n",name.c_str()) ;
  }else {
    TRACE(L"event_object failed to create. [name=%s]\r\n",name.c_str()) ;
  }
#endif
}
//---------------------------------------------------------------------------
event_object::event_object(const event_object &clone_source)
{
  name = clone_source.name ;
  event = clone_source.open() ;
#ifdef _DEBUG
  if(event) {
    TRACE(L"event_object cloned. [name=%s]\r\n",name.c_str()) ;
  }else {
    TRACE(L"event_object failed to clone. [name=%s]\r\n",name.c_str()) ;
  }
#endif
}
//---------------------------------------------------------------------------
event_object::~event_object()
{
  if(event) CloseHandle(event) ;
#ifdef _DEBUG
  if(event) {
    TRACE(L"event_object finished. [name=%s]\r\n",name.c_str()) ;
  }else {
    TRACE(L"event_object finished. (failure) [name=%s]\r\n",name.c_str()) ;
  }
#endif
}
//---------------------------------------------------------------------------
HANDLE event_object::open() const
{
  if(!event) return NULL ;
  HANDLE open_event = OpenEvent(EVENT_ALL_ACCESS, FALSE, name.c_str());
  return open_event ;
}
//---------------------------------------------------------------------------
DWORD event_object::wait(DWORD timeout) const
{
  return event ? WaitForSingleObject(event,timeout) : WAIT_FAILED ;
}
//---------------------------------------------------------------------------
BOOL event_object::set() const
{
  return event ? SetEvent(event) : FALSE ;
}
//---------------------------------------------------------------------------
BOOL event_object::reset() const
{
  return event ? ResetEvent(event) : FALSE ;
}
//---------------------------------------------------------------------------
BOOL event_object::pulse() const
{
  return event ? PulseEvent(event) : FALSE ;
}
//===========================================================================
// critical_object
//---------------------------------------------------------------------------
critical_object::critical_object()
{
  ref = new critical_ref_t ;
  InitializeCriticalSection(&ref->critical) ;
}
//---------------------------------------------------------------------------
critical_object::critical_object(const critical_object &clone_source)
{
  ref = clone_source.ref ;
  ref->ref_count++ ;
}
//---------------------------------------------------------------------------
critical_object::~critical_object()
{
  if(!--ref->ref_count) {
    DeleteCriticalSection(&ref->critical) ;
    delete ref ;
  }
}
//---------------------------------------------------------------------------
void critical_object::enter()
{
  EnterCriticalSection(&ref->critical) ;
}
//---------------------------------------------------------------------------
BOOL critical_object::try_enter()
{
  return TryEnterCriticalSection(&ref->critical) ;
}
//---------------------------------------------------------------------------
void critical_object::leave()
{
  LeaveCriticalSection(&ref->critical) ;
}
//===========================================================================
// CAsyncFifo
//---------------------------------------------------------------------------
CAsyncFifo::CAsyncFifo(
  size_t initialPool, size_t maximumPool, size_t emptyBorder,
  size_t packetSize, DWORD threadWait)
  : MaximumPool(max(1,max(initialPool,maximumPool))),
    LastFragment(false),
    Indices(MaximumPool),
    EmptyIndices(MaximumPool),
    EmptyBorder(emptyBorder),
    PacketSize(packetSize),
    THREADWAIT(threadWait),
    AllocThread(NULL),
    AllocEvent(FALSE),
    Terminated(false)
{
    initialPool = max(1,initialPool) ;
    DWORD flag = HEAP_NO_SERIALIZE ;
    // ヒープ作成
    Heap = HeapCreate(flag, 0, 0);
    // バッファ初期化
    PacketSize = packetSize ;
    BufferPool.resize(MaximumPool);
    BufferPool.set_heap(Heap) ;
    BufferPool.set_heap_flag(flag) ;
    for(size_t i = 0UL ; i < initialPool ; i++){
        BufferPool[i].resize(PacketSize);
        EmptyIndices.push(i) ;
    }
    // アロケーションスレッド作成
    if(maximumPool>initialPool) {
      AllocThread = (HANDLE)_beginthreadex(NULL, 0, AllocThreadProc, this, CREATE_SUSPENDED, NULL) ;
      if(AllocThread == 0) {
          AllocThread = NULL;
      }else{
          ::ResumeThread(AllocThread) ;
      }
    }
}
//---------------------------------------------------------------------------
CAsyncFifo::~CAsyncFifo()
{
    // アロケーションスレッド破棄
    Terminated=true ;
    bool abnormal=false ;
    if(AllocThread) {
      AllocEvent.set() ;
      if(::WaitForSingleObject(AllocThread,30000) != WAIT_OBJECT_0) {
        ::TerminateThread(AllocThread, 0);
        abnormal=true ;
      }
      ::CloseHandle(AllocThread) ;
    }

    // バッファ放棄（直後にヒープ自体を破棄するのでメモリリークは発生しない）
    BufferPool.abandon_erase(Heap) ;
    // ヒープ破棄
    if(!abnormal) HeapDestroy(Heap) ;
}
//---------------------------------------------------------------------------
size_t CAsyncFifo::Push(const BYTE *data, DWORD len)
{
  exclusive_lock plock(&PurgeExclusive) ;

  size_t sz=0, n=0 ;
  for(BYTE *p = const_cast<BYTE*>(data) ; len ; len-=(DWORD)sz, p+=sz) {

    sz=min(len,PacketSize) ;

    size_t index ;
    if(LastFragment) { // 断片化されている場合は、空き領域の先頭が書込み途中
      index = EmptyIndices.front() ;
    }else {
      exclusive_lock lock(&Exclusive) ;
      if(EmptyIndices.empty()) {
        // 空きがない rotate (データは破壊されドロップが確実に発生する)
        index = Indices.front() ;
        Indices.pop();
        EmptyIndices.push(index) ;
      }else {
        // 空き位置取得
        index = EmptyIndices.front() ;
      }
    }

    // The number of empty indices is under the limit, or not.
    if (EmptyIndices.size()<EmptyBorder) {
      // Allocation ordering...
      AllocEvent.set() ;
    }

    // リサイズとデータ書き(No lock)
    if(LastFragment) { // 断片化が起きている
      size_t buf_sz = BufferPool[index].size() ;
      sz = min(sz,PacketSize-buf_sz) ;
      BufferPool[index].resize(buf_sz+sz) ;
      CopyMemory(BufferPool[index].top()+buf_sz, p, sz );
    }else {
      BufferPool[index].resize(sz) ;
      CopyMemory(BufferPool[index].top(), p, sz );
    }
    LastFragment = BufferPool[index].size()<PacketSize ;

    // FIFOバッファにプッシュ
    if(!LastFragment) {
      exclusive_lock lock(&Exclusive) ;
      EmptyIndices.pop() ;
      Indices.push(index);
      n++ ;
    }

  }
  return n ;
}
//---------------------------------------------------------------------------
bool CAsyncFifo::Pop(BYTE **data, DWORD *len,DWORD *remain)
{
    exclusive_lock lock(&Exclusive);
    if(Empty()) {
      if(len) *len = 0 ;
      if(data) *data = 0 ;
      if(remain) *remain = 0 ;
      return false ;
    }
    size_t index = Indices.front() ;
    if(len) *len = (DWORD)BufferPool[index].size() ;
    if(data) *data = (BYTE*)BufferPool[index].top() ;
    EmptyIndices.push(index) ;
    Indices.pop();
    if(remain) *remain = (DWORD)Size() ;
    return true;
}
//---------------------------------------------------------------------------
void CAsyncFifo::Purge()
{
    // バッファから取り出し可能データをパージする
    exclusive_lock plock(&PurgeExclusive) ;
    exclusive_lock lock(&Exclusive);

    // 未処理のデータをパージする
    while(!Indices.empty()) {
        EmptyIndices.push(Indices.front()) ;
        Indices.pop() ;
    }
    LastFragment = false ;
}
//---------------------------------------------------------------------------
unsigned int CAsyncFifo::AllocThreadProcMain ()
{
    for(;;) {
      DWORD dwRet = AllocEvent.wait(THREADWAIT);
      if (Terminated) break;
      if(dwRet==WAIT_FAILED) return 1 ;
      else if(dwRet == WAIT_OBJECT_0) { // Allocation ordered
        Exclusive.lock() ;
        size_t nEmpty = EmptyIndices.size() ;
        size_t nFifo = Indices.size() ;
        Exclusive.unlock();
        // total : nFifo + nEmpty
        while(nFifo+nEmpty<MaximumPool&&nEmpty<EmptyBorder) {
          BufferPool[nFifo + nEmpty].resize(PacketSize); // Allocating...
          Exclusive.lock();
          EmptyIndices.push(nFifo+nEmpty) ;
          nEmpty = EmptyIndices.size() ;
          nFifo = Indices.size() ;
          Exclusive.unlock();
          if (Terminated) break;
        }
        DBGOUT("Async FIFO allocation: total %zu bytes grown.\r\n",(nFifo+nEmpty)*PacketSize) ;
      }
      if (Terminated) break;
    }
    return 0 ;
}
//---------------------------------------------------------------------------
unsigned int __stdcall CAsyncFifo::AllocThreadProc (PVOID pv)
{
    register CAsyncFifo *_this = static_cast<CAsyncFifo*>(pv) ;
    unsigned int result = _this->AllocThreadProcMain() ;
    _endthreadex(result) ;
    return result;
}
//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
