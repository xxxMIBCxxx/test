#pragma once
//*****************************************************************************
// �ڑ��Ď��X���b�h�N���X�i�N���C�A���g�Łj
//*****************************************************************************
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "CThread.h"
#include "CClientConnectMonitoringThread.h"
#include "CTcpRecvThread.h"


class CClientConnectMonitoringThread : public CThread
{
public:
	// �ڑ��Ď��X���b�h�N���X�i�N���C�A���g�Łj�̌��ʎ��
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,											// ����I��
		RESULT_ERROR_INIT = 0xE00000001,										// ���������Ɏ��s���Ă���
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,								// ���ɃX���b�h���J�n���Ă���
		RESULT_ERROR_START = 0xE00000003,										// �X���b�h�J�n�Ɏ��s���܂���

		RESULT_ERROR_PARAM = 0xE1000001,										// �p�����[�^�G���[
		RESULT_ERROR_CREATE_SOCKET = 0xE1000002,								// �\�P�b�g�����Ɏ��s
		RESULT_ERROR_CONNECT = 0xE1000003,										// �ڑ��Ɏ��s
		RESULT_ERROR_LISTEN = 0xE1000004,										// �ڑ��҂��Ɏ��s

		RESULT_ERROR_SYSTEM = 0xE9999999,										// �V�X�e���G���[
	} RESULT_ENUM;


	// �T�[�o�[���\����
	typedef struct
	{
		int									Socket;								// �\�P�b�g
		struct sockaddr_in					tAddr;								// �C���^�[�l�b�g�\�P�b�g�A�h���X�\����
	} SERVER_INFO_TABLE;


	CTcpRecvThread*							m_pcTcpRecvThread;					// TCP��M�X���b�h
	CEvent*									m_pcServerDisconnectEvent;			// �T�[�o�[�ؒf�C�x���g

private:
	bool									m_bInitFlag;						// �����������t���O
	int										m_ErrorNo;							// �G���[�ԍ�

	SERVER_INFO_TABLE						m_tServerInfo;						// �T�[�o�[���
	char									m_szIpAddr[IP_ADDR_BUFF_SIZE + 1];	// IP�A�h���X
	uint16_t								m_Port;								// �|�[�g�ԍ�
	int										m_epfd;								// epoll�t�@�C���f�B�X�N���v�^�i�N���C�A���g�����X���b�h�Ŏg�p�j

public:
	CClientConnectMonitoringThread(CEvent* pcServerDisconnectEvent);
	~CClientConnectMonitoringThread();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();

	RESULT_ENUM ServerConnect(SERVER_INFO_TABLE& tServerInfo);
	void ServerDisconnect(SERVER_INFO_TABLE& tServerInfo);

private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	RESULT_ENUM CreateTcpThread(SERVER_INFO_TABLE& tServerInfo);
	void DeleteTcpThread();
};








