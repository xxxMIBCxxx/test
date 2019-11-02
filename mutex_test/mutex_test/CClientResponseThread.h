#pragma once
//*****************************************************************************
// �N���C�A���g�����X���b�h�N���X
//*****************************************************************************
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "CThread.h"
#include "CEvent.h"
#include "CTcpSendThread.h"
#include "CTcpRecvThread.h"


#define	IP_ADDR_BUFF_SIZE					( 32 )
#define RECV_BUFF_SIZE						( 1024 )


class CClientResponseThread : public CThread
{
public:

	// �N���C�A���g�����X���b�h�N���X�̌��ʎ��
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,											// ����I��
		RESULT_ERROR_INIT = 0xE00000001,										// ���������Ɏ��s���Ă���
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,								// ���ɃX���b�h���J�n���Ă���
		RESULT_ERROR_START = 0xE00000003,										// �X���b�h�J�n�Ɏ��s���܂���

		RESULT_ERROR_SYSTEM = 0xE9999999,										// �V�X�e���G���[
	} RESULT_ENUM;

	// �N���C�A���g���\����
	typedef struct
	{
		int									Socket;								// �\�P�b�g
		struct sockaddr_in					tAddr;								// �C���^�[�l�b�g�\�P�b�g�A�h���X�\����

	} CLIENT_INFO_TABLE;



	bool									m_bInitFlag;						// �����������t���O
	int										m_ErrorNo;							// �G���[�ԍ�

	CLIENT_INFO_TABLE						m_tClientInfo;						// �N���C�A���g���
	char									m_szIpAddr[IP_ADDR_BUFF_SIZE + 1];	// IP�A�h���X
	uint16_t								m_Port;								// �|�[�g�ԍ�
	
	int										m_epfd;								// epoll�t�@�C���f�B�X�N���v�^�i�N���C�A���g�����X���b�h�Ŏg�p�j
	char									m_szRecvBuf[RECV_BUFF_SIZE + 1];	// ��M�o�b�t�@

	CTcpSendThread*							m_pcTcpSendThread;					// TCP���M�X���b�h
	CTcpRecvThread*							m_pcTcpRecvThread;					// TCP��M�X���b�h

	bool									m_bClientResponseThread_EndFlag;	// �N���C�A���g�����X���b�h�I���t���O
	CEvent*									m_pcClientResponseThread_EndEvent;	// �N���C�A���g�����X���b�h�I���C�x���g

public:
	CClientResponseThread(CLIENT_INFO_TABLE& tClientInfo, CEvent* pcClientResponseThread_EndEvent);
	~CClientResponseThread();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	bool IsClientResponseThreadEnd();

public:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);

};








