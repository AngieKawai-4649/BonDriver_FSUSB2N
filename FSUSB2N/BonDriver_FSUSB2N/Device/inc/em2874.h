#pragma once

#include <WinUSB.h>

typedef unsigned char	uint8_t;
typedef unsigned short	uint16_t;
typedef unsigned int	uint32_t;
typedef signed int		int32_t;

#define EM28XX_REG_I2C_CLK		0x06
#define EM2874_REG_TS_ENABLE	0x5f
#define EM28XX_REG_GPIO			0x80

#define EM2874_REG_CAS_STATUS	0x70
#define EM2874_REG_CAS_DATALEN	0x71
#define EM2874_REG_CAS_MODE1	0x72
#define EM2874_REG_CAS_RESET	0x73
#define EM2874_REG_CAS_MODE2	0x75

#define EM2874_EP_TS1		0x84

#define DEMOD_ADDR	0x20
#define EEPROM_ADDR	0xa0
#define TUNER_ADDR	0xc0

/* EM2874 TS Enable Register (0x5f) */
#define EM2874_TS1_CAPTURE_ENABLE 0x01
#define EM2874_TS1_FILTER_ENABLE  0x02
#define EM2874_TS1_NULL_DISCARD   0x04

#define EM2874_TS

#ifdef EM2874_TS
	#define USBBULK_XFERSIZE	(0x20000) // one block 128K bytes ( Optimum for Spinel )
    #define RINGBUFF_SIZE	20 // 128K * 12 = 2.5M bytes
    #define NUM_IOHANDLE	12 // 12 blocks ( 1.5M bytes ) of 2.5Mbyts for queuing
#endif

typedef struct _TSIO_CONTEXT {

    OVERLAPPED ol;
	unsigned index;

	//_TSIO_CONTEXT(void) :index(0) {}

} TSIO_CONTEXT;

class EM2874Device
{
private:
	EM2874Device ();
	bool resetICC_1 ();
	bool resetICC_2 ();

	HANDLE dev;
	WINUSB_INTERFACE_HANDLE usbHandle;
	uint8_t cardPCB;

#ifdef EM2874_TS
	int BeginAsyncRead();
	int GetOverlappedResult();

	uint8_t *TsBuff;
	volatile int32_t *TsBuffSize;
	int TsBuffIndex;
	int OverlappedIoIndex;
	HANDLE hTsEvent;
	TSIO_CONTEXT IoContext[NUM_IOHANDLE];
	ULONGLONG dwtLastRead;
#endif

public:
	EM2874Device (HANDLE hDev);
	~EM2874Device ();
	static EM2874Device* AllocDevice(int idx);
	bool initDevice ();
	bool initDevice2 () const;

	uint8_t readReg (const uint8_t idx) const;
	bool readReg (const uint8_t idx, uint8_t *val) const;
	bool writeReg (const uint8_t idx, const uint8_t val) const;
	bool readI2C (const uint8_t addr, const uint16_t size, uint8_t *data, const bool isStop) const;
	bool writeI2C (const uint8_t addr, const uint16_t size, uint8_t *data, const bool isStop) const;

	WINUSB_INTERFACE_HANDLE getUsbHandle() const;

#ifdef EM2874_ICC
	bool resetICC ();
	bool writeICC (const size_t size, const void *data);
	bool readICC (size_t *size, void *data);
	int  waitICC();
	int  getCardStatus();
#endif
	int  getDeviceID() const;

	bool isCardReady;
	static unsigned UserSettings;

#ifdef EM2874_TS
	void SetBuffer(void *pBuf);
	bool TransferStart();
	void TransferStop();
	void TransferPause() const;
	void TransferResume() const;
	int DispatchTSRead(BOOL *bNextReadIn=NULL);
	HANDLE GetHandle() const;
#endif
};
// Fixed by ÅüPRY8EAlByw
// hyrolean-dtv@yahoo.co.jp