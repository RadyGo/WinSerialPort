#include "StdAfx.h"
#include <time.h>
#include "..\..\include\SerialPort.h"

using namespace std;

SerialPort::SerialPort()
	:
	portName(""),
	baudrate(9600),
	parity(ParityNone),
	stopbits(StopBitOne),
	databits(8),
	hRs232c(INVALID_HANDLE_VALUE)
{
}
SerialPort::~SerialPort()
{
}
bool SerialPort::Open(int timeout/* = 0*/)
{
    memset(SndBuf,0,sizeof(SndBuf));
    memset(RcvBuf,0,sizeof(RcvBuf));

	string filename = string("\\\\.\\").append(portName);

    // COM�|�[�g�̃I�[�v��
    hRs232c = CreateFile(
        filename.c_str(),
		GENERIC_READ|GENERIC_WRITE,0,NULL,
        OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL
#ifdef OVERLAPPED_IO
		|FILE_FLAG_OVERLAPPED
#endif
		,NULL);
    if ( hRs232c == INVALID_HANDLE_VALUE ) {
        printf("RS232C CreateFile Error\n");
        return false;
    }


    // COM�|�[�g�̃Z�b�g�A�b�v
    if (SetupComm(hRs232c,sizeof(RcvBuf),sizeof(SndBuf)) == FALSE){
        printf("RS232C SetupComm Error\n");
        return false;
    }
	
    // COM�|�[�g�̐ݒ�擾
    DCB dcb;
    memset(&dcb, NULL, sizeof(DCB));
    dcb.DCBlength = sizeof(DCB);
    GetCommState(hRs232c, &dcb );

    // COM�|�[�g�̐ݒ�ύX
    dcb.BaudRate = baudrate;
    dcb.Parity   = (int)parity;
    dcb.StopBits = (int)stopbits;
    dcb.ByteSize = databits;
    if (SetCommState(hRs232c,&dcb) == FALSE){
        printf("RS232C SetCommState Error\n");
        return false;
    }
	COMMTIMEOUTS tmo;
	tmo.ReadTotalTimeoutConstant = timeout;
	tmo.ReadTotalTimeoutMultiplier = 0;
	tmo.WriteTotalTimeoutConstant = timeout;
	tmo.WriteTotalTimeoutMultiplier = 0;
	if(SetCommTimeouts(hRs232c, &tmo) == FALSE) {
        printf("RS232C SetCommTimeouts Error\n");
		return false;
	}

#ifdef OVERLAPPED_IO
    // COM�|�[�g�̎�M�C�x���g�쐬
    memset(&ovRead,0,sizeof(ovRead));
    ovRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ovRead.hEvent == NULL){
        printf("RS232C CreateEvent Error\n");
        return 0;
    }
    // COM�|�[�g�̎�M�C�x���g�}�X�N
    if (SetCommMask(hRs232c, EV_RXCHAR) == FALSE){
        printf("RS232C SetCommMask Error\n");
        return 0;
    }
#endif
	return true;
}
void SerialPort::Close()
{
#ifdef OVERLAPPED_IO
    // OM�|�[�g�̎�M�C�x���g�N���[�Y
    CloseHandle(ovRead.hEvent);
#endif

    // COM�|�[�g�̃N���[�Y
    CloseHandle(hRs232c);
	hRs232c = INVALID_HANDLE_VALUE;
}
int SerialPort::Write(const void* buf, int offset, int count)
{
	const unsigned char* sendbuf = (const unsigned char*)buf;
    DWORD dwSize = 0;
	OVERLAPPED* pOverlapped = 0;

#ifdef OVERLAPPED_IO
    OVERLAPPED ovWrite;
    memset(&ovWrite,0,sizeof(ovWrite));
	pOverlapped = &ovWrite;
#endif

    if (WriteFile(hRs232c, &sendbuf[offset], count, &dwSize, pOverlapped) == FALSE) {
        printf("RS232C WriteFile Error\n");
        return 0;
    }
	return (int)dwSize;
}
int SerialPort::Read(void* buf, int offset, int count)
{
	unsigned char* recvbuf = (unsigned char*)buf;
	DWORD dwSize = 0;
	OVERLAPPED* pOverlapped = 0;

#ifdef OVERLAPPED_IO
	pOverlapped = &ovRead;

    // COM�|�[�g�̎�M�҂�
    DWORD dwEvtMask = 0;
    DWORD dwTransfer = 0;
	clock_t wait_start = clock();
    if (WaitCommEvent(hRs232c, &dwEvtMask, pOverlapped) == FALSE){
        if (GetLastError() == ERROR_IO_PENDING) {
            GetOverlappedResult(hRs232c, pOverlapped, &dwTransfer, TRUE);
        }else{
            printf("RS232C WaitCommEvent Error\n");
            return 0;
        }
    }
    // COM�|�[�g�̎�M�C�x���g�`�F�b�N
    if ((dwEvtMask & EV_RXCHAR) != EV_RXCHAR){
		return 0;
	}
    // COM�|�[�g�̎�M�f�[�^�`�F�b�N
    COMSTAT comst;
    DWORD dwErr;
    ClearCommError(hRs232c, &dwErr, &comst);
	if(comst.cbInQue < count) {
		return 0;
	}
#endif
    // COM�|�[�g����f�[�^��M
    if(ReadFile(hRs232c, &recvbuf[offset], count, &dwSize, pOverlapped) == FALSE){
        printf("RS232C ReadFile Error\n");
        return -1;
    }
	if(count != 0 && dwSize == 0) {
		return -1;
	}
	return (int)dwSize;
}
