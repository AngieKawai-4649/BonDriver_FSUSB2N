#pragma once

// BonTuner.h: CBonTuner クラスのインターフェイス
//////////////////////////////////////////////////

#include "IBonDriver2.h"
#include "ktv.h"
#include "pryutil.h"
#include "ParseChSet.h"
#include "IB25Decoder.h"


class CBonTuner : public IBonDriver2
{
public:
	CBonTuner();
	virtual ~CBonTuner();

// IBonDriver
	const BOOL OpenTuner(void);
	void CloseTuner(void);

	const BOOL SetChannel(const BYTE bCh);
	const float GetSignalLevel(void);

	const DWORD WaitTsStream(const DWORD dwTimeOut = 0);
	const DWORD GetReadyCount(void);

	const BOOL GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain);
	const BOOL GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain);

	void PurgeTsStream(void);

// IBonDriver2(暫定)
	LPCTSTR GetTunerName(void);

	const BOOL IsTunerOpening(void);

	LPCTSTR EnumTuningSpace(const DWORD dwSpace);
	LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel);

	const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel);

	const DWORD GetCurSpace(void);
	const DWORD GetCurChannel(void);

	void Release(void);

	static CBonTuner * m_pThis;
	static HINSTANCE m_hModule;

protected:
	DWORD m_dwCurSpace;
	DWORD m_dwCurChannel;

	EM2874Device *usbDev;
	KtvDevice *pDev;
	BYTE *m_pTsBuff;
	volatile int *m_TsBuffSize;		// 32bit signed
	volatile int m_indexTsBuff;

	HANDLE m_hThread;
    HANDLE m_hTsRecv;
    unsigned int TsThreadMain () ;
	static unsigned int __stdcall TsThread (PVOID pv);
    // Asynchronized fifo buffering based contexts
    CAsyncFifo *m_AsyncTSFifo ;
    exclusive_object m_exSuspend ;
    HANDLE m_evThreadSuspend ;
    HANDLE m_evThreadResume ;
    int m_cntThreadSuspend ;
    void ThreadSuspend() ;
    void ThreadResume() ;

private:
    int m_iTunerID;							// BonDriver_FSUSB2N-X.ini のX（チューナー番号）をセット 0スタート 指定無しは-1
    CParseChSet m_chSet;					// iniファイル データ格納エリア
	DWORD m_dwSetTunerOpenRetryMaxCnt;		// チューナーオープンリトライ回数 default 5
	DWORD m_dwSetTunerOpenRetryWaitTime;	// チューナーオープンリトライ間隔（ミリ秒で指定） default 50

	DWORD m_SetTunerIdWaitTime;				// デバイスを初期化してから使用チューナーID (TDA18271HD or MXL135RF) を取得するまでの待ち時間（default 80）
	DWORD m_SetTunerStandbyInitWaitTime;	// チューナーをスタンバイモードにしてから初期化するまでの待ち時間 (default 80)
	DWORD m_SetTunerInitDemoWaitTime;		// チューナーを初期化してからデモジュレーターを初期化するまでの待ち時間 (default 80)
	DWORD m_SetDemoEndWaitTime;				// デモジュレーターをリセットしてからプログラムの制御を戻すまでの待ち時間 (default 300)

	DWORD m_SetResetDemoWaitTime;			// 地デジ：チューナー周波数を設定してからデモジュレーターをリセットするまでの時間（ミリ秒） (default 5)
	DWORD m_SetDemoWaitTime;				// デモジュレーターをリセットしてからTSストリームを抽出をするまでの待ち時間（ミリ秒） (default 20)
	DWORD m_SetTsResumeWaitTime;			// TSストリームを抽出してからTS受信スレッドを再開するまでも待ち時間（ミリ秒） (default 10)

	HMODULE m_hDecoderModule;
	IB25Decoder2* (*m_CreateInstance)();
	IB25Decoder2* m_b25decoderIF;
	BYTE m_nullPacket;
	BYTE m_multi2Round;

	UINT m_CardResponseTime;
};
