#pragma once

// BonTuner.h: CBonTuner �N���X�̃C���^�[�t�F�C�X
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

// IBonDriver2(�b��)
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
    int m_iTunerID;							// BonDriver_FSUSB2N-X.ini ��X�i�`���[�i�[�ԍ��j���Z�b�g 0�X�^�[�g �w�薳����-1
    CParseChSet m_chSet;					// ini�t�@�C�� �f�[�^�i�[�G���A
	DWORD m_dwSetTunerOpenRetryMaxCnt;		// �`���[�i�[�I�[�v�����g���C�� default 5
	DWORD m_dwSetTunerOpenRetryWaitTime;	// �`���[�i�[�I�[�v�����g���C�Ԋu�i�~���b�Ŏw��j default 50

	DWORD m_SetTunerIdWaitTime;				// �f�o�C�X�����������Ă���g�p�`���[�i�[ID (TDA18271HD or MXL135RF) ���擾����܂ł̑҂����ԁidefault 80�j
	DWORD m_SetTunerStandbyInitWaitTime;	// �`���[�i�[���X�^���o�C���[�h�ɂ��Ă��珉��������܂ł̑҂����� (default 80)
	DWORD m_SetTunerInitDemoWaitTime;		// �`���[�i�[�����������Ă���f���W�����[�^�[������������܂ł̑҂����� (default 80)
	DWORD m_SetDemoEndWaitTime;				// �f���W�����[�^�[�����Z�b�g���Ă���v���O�����̐����߂��܂ł̑҂����� (default 300)

	DWORD m_SetResetDemoWaitTime;			// �n�f�W�F�`���[�i�[���g����ݒ肵�Ă���f���W�����[�^�[�����Z�b�g����܂ł̎��ԁi�~���b�j (default 5)
	DWORD m_SetDemoWaitTime;				// �f���W�����[�^�[�����Z�b�g���Ă���TS�X�g���[���𒊏o������܂ł̑҂����ԁi�~���b�j (default 20)
	DWORD m_SetTsResumeWaitTime;			// TS�X�g���[���𒊏o���Ă���TS��M�X���b�h���ĊJ����܂ł��҂����ԁi�~���b�j (default 10)

	HMODULE m_hDecoderModule;
	IB25Decoder2* (*m_CreateInstance)();
	IB25Decoder2* m_b25decoderIF;
	BYTE m_nullPacket;
	BYTE m_multi2Round;

	UINT m_CardResponseTime;
};
