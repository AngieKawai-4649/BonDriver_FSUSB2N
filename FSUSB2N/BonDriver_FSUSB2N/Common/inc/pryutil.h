//===========================================================================
#pragma once
#ifndef _PRYUTIL_20141218222525309_H_INCLUDED_
#define _PRYUTIL_20141218222525309_H_INCLUDED_
//---------------------------------------------------------------------------

#include <Windows.h>
#include <assert.h>
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <stdexcept>

typedef std::basic_string<_TCHAR> tstring;
#if defined(UNICODE) || defined(_UNICODE)
# define titos itows
#else
# define titos itos
#endif

//===========================================================================
namespace PRY8EAlByw {
//===========================================================================
// Functions
//---------------------------------------------------------------------------
ULONGLONG PastSleep(ULONGLONG wait=0,ULONGLONG start=GetTickCount64()) ;

std::wstring mbcs2wcs(std::string src);
std::string wcs2mbcs(std::wstring src);

std::string itos(int val,int radix=10);
std::wstring itows(int val,int radix=10);

std::string upper_case(std::string str) ;
std::string lower_case(std::string str) ;

std::string file_path_of(std::string filename);
std::string file_prefix_of(std::string filename);

//===========================================================================
// Inline Functions
//---------------------------------------------------------------------------
template<class String> String inline trim(const String &str)
{
    String str2{}; str2.clear();
  for(typename String::size_type i=0;i<str.size();i++) {
    if(unsigned(str[i])>0x20UL) {
      str2 = str.substr(i,str.size()-i) ;
      break ;
    }
  }
  if(str2.empty()) return str2 ;
  for(typename String::size_type i=str2.size();i>0;i--) {
    if(unsigned(str2[i-1])>0x20UL) {
      return str2.substr(0,i) ;
    }
  }
  str2.clear() ;
  return str2 ;
}
//---------------------------------------------------------------------------
#if 0
template<class Container> void inline split(
	Container &DivStrings, const typename Container::value_type &Text,
	typename Container::value_type::value_type Delimiter)
#else
template<class Container,class String> void inline split(
	Container &DivStrings/*string container*/, const String &Text,
	typename String::value_type Delimiter)
#endif
{
  #ifdef _DEBUG
  assert(typeid(typename Container::value_type)==typeid(Text));
  #endif
  typename Container::value_type temp{}; temp.clear();
  for(typename Container::value_type::size_type i=0;i<Text.size();i++) {
    if(Text[i]==Delimiter) {
      DivStrings.push_back(trim(temp));
      temp.clear();
      continue;
    }
    temp+=Text[i];
  }
  if(!trim(temp).empty()) {
     DivStrings.push_back(trim(temp));
  }
}
//---------------------------------------------------------------------------
//===========================================================================
// Classes
//---------------------------------------------------------------------------

  // 簡易イベントオブジェクト

class event_object
{
private:
  HANDLE event ;
  tstring name ;
public:
  event_object(BOOL _InitialState=TRUE/*default:signalized*/, tstring _name = _T("")) ;
  event_object(const event_object &clone_source) ; // 現スレッドでイベントを複製
  ~event_object() ;
  //std::wstring event_name() const { return name ; }
  HANDLE handle() const { return event ; }
  // 現スレッドの下で複製のイベントハンドルを開く
  HANDLE open() const ;
  // 現スレッドでシグナル化されるまでブロックし、解除された後、自動的に再び非シグナル化
  DWORD wait(DWORD timeout=INFINITE) const;
  // シグナル化（ブロックされているスレッドが解除されるのは複数のうちの一つ）
  BOOL set() const;
  // マニュアル的な非シグナル化
  BOOL reset() const;
  // シグナル化（ブロックされている全スレッドの一括解除）
  BOOL pulse() const;
  // lock(wait) / unlock(set)
  DWORD lock(DWORD timeout=INFINITE) const{ return wait(timeout) ; }
  BOOL unlock() const{ return set() ; }

};


  // 簡易クリティカルオブジェクト

class critical_object
{
private:
  struct critical_ref_t {
      CRITICAL_SECTION critical{};
      int ref_count{1};
    //critical_ref_t(void) : ref_count(1){}

    critical_ref_t(void) {}

  };
  critical_ref_t *ref ;
public:
  critical_object() ;
  critical_object(const critical_object &clone_source) ;
  ~critical_object() ;
  CRITICAL_SECTION *handle() const { return &ref->critical ; }
  // wrapper
  void enter() ;
  BOOL try_enter() ;
  void leave() ;
  // lock / unlock
  void lock() { enter() ; }
  void unlock() { leave() ; }
};


  // スコープレベルの自動ロック／アンロック(オブジェクト参照レベル)

template <typename locker_t>
class basic_lock
{
private:
  bool unlocked ;
  locker_t *locker_ref ;
public:
  basic_lock(locker_t *source_ref)
    : locker_ref(source_ref) {
    // 非シグナル状態が解除されるまで現スレッドをブロック
    unlocked = false ;
    locker_ref->lock() ;
  }
  ~basic_lock() {
    // スコープ終了時点で非シグナル状態を自動解除する
    unlock() ;
  }
  void unlock() {
    // スコープの途中でこのメソッド呼び出すとその時点で非シグナル状態を解除する
    if(!unlocked) {
      // シグナル状態にセット
      locker_ref->unlock() ;
      unlocked = true ;
    }
  }
};
typedef basic_lock<event_object> event_lock ;
typedef basic_lock<critical_object> critical_lock ;


  // スコープレベルの自動ロック／アンロック(オブジェクト複製レベル)

template <typename locker_t>
class basic_lock_object
{
private:
  locker_t locker ;
  bool unlocked ;
public:
  basic_lock_object(const locker_t &source_object)
    : locker(source_object) {
    // 非シグナル状態が解除されるまで現スレッドをブロック
    unlocked = false ;
    locker.lock() ;
  }
  ~basic_lock_object() {
    // スコープ終了時点で非シグナル状態を自動解除する
    unlock() ;
  }
  void unlock() {
    // スコープの途中でこのメソッド呼び出すとその時点で非シグナル状態を解除する
    if(!unlocked) {
      // シグナル状態にセット
      locker.unlock() ;
      unlocked = true ;
    }
  }
};
typedef basic_lock_object<event_object> event_lock_object ;
typedef basic_lock_object<critical_object> critical_lock_object ;


  // exclusive

  #if 0  // ｲﾍﾞﾝﾄ
  typedef event_object exclusive_object ;
  #else  // ｸﾘﾃｨｶﾙ ｾｸｼｮﾝ
  typedef critical_object exclusive_object ;
  #endif
  typedef basic_lock<exclusive_object> exclusive_lock ;
  typedef basic_lock_object<exclusive_object> exclusive_lock_object ;


  // BUFFER/BUFFERPOOL

template<typename T>
struct BUFFER {
    typedef size_t size_type ;
    typedef T  value_type ;
    typedef T& reference_type ;
    typedef T* pointer_type ;
    BUFFER(size_type size=0,HANDLE heap=NULL,DWORD heap_flag=0) : buffer_(NULL), size_(0UL), grow_(0UL) {
      heap_ = heap ? heap : GetProcessHeap() ;
      heap_flag_ = heap_flag ;
      if(size>0) resize(size) ;
    }
    BUFFER(const BUFFER<T> &src)
     : buffer_(NULL), size_(0UL), grow_(0UL), heap_(src.heap_), heap_flag_(src.heap_flag_) {
      *this = src ;
    }
    BUFFER(const void *buffer, size_type size,HANDLE heap=NULL)
     : buffer_(NULL), size_(0UL), grow_(0UL) {
      heap_ = heap ? heap : GetProcessHeap() ;
      if(size>0) resize(size) ;
      if(buffer_&&size_==size)
        CopyMemory(buffer_,buffer,size*sizeof(value_type)) ;
    }
    ~BUFFER() {
      clear() ;
    }
    BUFFER &operator =(const BUFFER<T> &src) {
      if(heap_!=src.heap_) {
        clear() ;
        heap_ = src.heap_ ;
      }
      heap_flag_ = src.heap_flag_ ;
      resize(src.size_) ;
      if(buffer_&&size_==src.size_)
        CopyMemory(buffer_,src.buffer_,src.size_*sizeof(value_type)) ;
      return *this ;
    }
    void clear() {
      if(buffer_) {
        HeapFree(heap_,heap_flag_&HEAP_NO_SERIALIZE,buffer_) ;
        buffer_=NULL ; grow_=0UL ; size_=0UL ;
      }
    }
    bool abandon(HANDLE heap) {
      if(heap_==heap) {
        if(buffer_) { buffer_=NULL ; grow_=0UL ; size_=0UL ; }
        return true ;
      }
      return false ;
    }
    void resize(size_type size) {
      if(size_!=size) {
        if(buffer_) {
          if(size>grow_) {
            buffer_ = (pointer_type)HeapReAlloc(heap_,
              heap_flag_&(HEAP_NO_SERIALIZE|HEAP_ZERO_MEMORY|HEAP_REALLOC_IN_PLACE_ONLY),
              buffer_,size*sizeof(value_type)) ;
            if(buffer_) grow_ = size ;
            else grow_ = 0UL ;
          }
        }else {
          buffer_ = (pointer_type)HeapAlloc(heap_,
            heap_flag_&(HEAP_NO_SERIALIZE|HEAP_ZERO_MEMORY),size*sizeof(value_type)) ;
          if(buffer_) grow_ = size ;
          else grow_ = 0UL ;
        }
        if(buffer_) size_ = size ;
        else size_ = 0UL ;
      }
    }
    reference_type operator[](size_type index) { return buffer_[index] ; }
    pointer_type top() { return buffer_ ; }
    size_type size() const { return size_ ; }
    void set_heap_flag(DWORD flag) { heap_flag_ = flag ; }
    void set_heap(HANDLE heap) {
      if(!heap) heap=GetProcessHeap() ;
      if(heap_!=heap) {
        size_type sz = size() ;
        if(sz) {
          pointer_type buffer = (pointer_type)HeapAlloc(heap,
            heap_flag_&(HEAP_NO_SERIALIZE|HEAP_ZERO_MEMORY),sz*sizeof(value_type)) ;
          if(buffer) {
            CopyMemory(buffer,buffer_,sz*sizeof(value_type)) ;
            clear() ;
            buffer_ = buffer ;
            size_ = sz ; grow_ = sz ;
          }else {
            clear() ;
          }
        }
        heap_ = heap ;
      }
    }
private:
    HANDLE heap_ ;
    DWORD heap_flag_ ;
    pointer_type buffer_ ;
    size_type size_ ;
    size_type grow_ ;
};

template<typename T,class Container=std::vector< BUFFER<T> > >
struct BUFFERPOOL {
  typedef BUFFER<T> value_type ;
  typedef BUFFER<T>* pointer_type ;
  typedef BUFFER<T>& reference_type ;
  typedef size_t size_type ;
  void set_heap(HANDLE heap) {
    std::for_each(cont_.begin(),cont_.end(),
      std::bind2nd(std::mem_fun_ref(&value_type::set_heap),heap));
  }
  void set_heap_flag(DWORD flag) {
    std::for_each(cont_.begin(),cont_.end(),
      std::bind2nd(std::mem_fun_ref(&value_type::set_heap_flag),flag));
  }
  void abandon_erase(HANDLE heap) {
    cont_.erase(std::remove_if(cont_.begin(),cont_.end(),
      std::bind2nd(std::mem_fun_ref(&value_type::abandon),heap)),cont_.end());
  }
  void resize(size_type size) {
    cont_.resize(size) ;
  }
  void clear() {
    cont_.clear() ;
  }
  size_type size() {
    return cont_.size() ;
  }
  reference_type operator[](size_t index) {
    return cont_[index] ;
  }
  Container &container() { return cont_ ; }
private:
  Container cont_ ;
};

  // fixed_queue (アロケーションが発生しない順列)

template<typename T>
class fixed_queue
{
public:
  typedef T value_type ;
  typedef T& reference_type ;
  typedef T* pointer_type ;
  typedef size_t size_type ;
private:
  pointer_type buffer_ ;
  size_type start_ ;
  size_type size_ ;
  size_type limit_ ;
public:
  fixed_queue(size_type max_size)
    : limit_(max_size), start_(0), size_(0) {
    buffer_ = new value_type[limit_] ;
  }
  ~fixed_queue() {
    delete [] buffer_ ;
  }
  size_type max_range() const { return limit_ ; }
  size_type size() const { return size_ ; }
  bool empty() const { return size_==0 ; }
  bool full() const { return size_>=limit_ ; }
  bool push(const value_type &val) {
    if(full()) return false ;
    buffer_[(start_+size_++)%limit_] = val ;
    return true ;
  }
  bool pop() {
    if(empty()) return false ;
    if(++start_>=limit_) start_ = 0 ;
    size_-- ;
    return true ;
  }
  reference_type front() {
  #ifdef _DEBUG
    if(empty()) throw std::range_error("fixed_queue: front() range error.") ;
  #endif
    return buffer_[start_] ;
  }
  void clear() { start_ = 0 ; size_ = 0 ; }
};

  // CAsyncFifo

class CAsyncFifo
{
private:
  size_t MaximumPool ;
  size_t EmptyBorder ;
  size_t PacketSize ;
  DWORD THREADWAIT ;
  exclusive_object Exclusive, PurgeExclusive ;
  HANDLE Heap ;
  bool LastFragment ;
  BUFFERPOOL<BYTE> BufferPool ;
  fixed_queue<size_t> Indices;
  fixed_queue<size_t> EmptyIndices;
  HANDLE AllocThread ;
  event_object AllocEvent ;
  unsigned int AllocThreadProcMain () ;
  static unsigned int __stdcall AllocThreadProc (PVOID pv) ;
  bool Terminated ;
public:
  CAsyncFifo(
    size_t initialPool, size_t maximumPool, size_t emptyBorder,
    size_t packetSize, DWORD threadWait=1000 ) ;
  virtual ~CAsyncFifo() ;
  size_t Size() const { return Indices.size() ; }
  bool Empty() const { return Indices.empty() ; }
  bool Full() const { return EmptyIndices.empty() ; }
  bool Growable() const { return Indices.size()+EmptyIndices.size()<MaximumPool ; }
  size_t Push(const BYTE *data, DWORD len) ;
  bool Pop(BYTE **data, DWORD *len, DWORD *remain) ;
  void Purge() ;
};

//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
using namespace PRY8EAlByw ;
//===========================================================================
#endif // _PRYUTIL_20141218222525309_H_INCLUDED_
