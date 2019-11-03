#pragma once
//*****************************************************************************
// TCP�ʐM��M�X���b�h�N���X�i�N���C�A���g�Łj
//*****************************************************************************
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "CThread.h"
#include "CEvent.h"
#include "CEventEx.h"
#include "CMutex.h"
#include "list"


#define CTCP_RECV_THREAD_RECV_BUFF_SIZE				( 100 )
#define CTCP_RECV_THREAD_COMMAND_BUFF_SIZE			( 1000 )
#define	IP_ADDR_BUFF_SIZE							( 32 )


class CTcpRecvThread : public CThread
{
public:
	// TCP�ʐM��M�X���b�h�N���X�̌��ʎ��
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,											// ����I��
		RESULT_ERROR_INIT = 0xE00000001,										// ���������Ɏ��s���Ă���
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,								// ���ɃX���b�h���J�n���Ă���
		RESULT_ERROR_START = 0xE00000003,										// �X���b�h�J�n�Ɏ��s���܂���

		RESULT_ERROR_NOT_ACTIVE = 0xE1000001,									// �X���b�h�����삵�Ă��Ȃ��i�܂��͏I�����Ă���j
		RESULT_ERROR_PARAM = 0xE1000002,										// �p�����[�^�G���[
		RESULT_ERROR_RECV = 0xE1000003,											// TCP��M�G���[
		RESULT_ERROR_COMMAND_BUFF_OVER = 0xE1000004,							// �R�}���h�o�b�t�@�I�[�o�[
		RESULT_ERROR_SYSTEM = 0xE9999999,										// �V�X�e���G���[
	} RESULT_ENUM;

	// �T�[�o�[���\����
	typedef struct
	{
		int									Socket;								// �\�P�b�g
		struct sockaddr_in					tAddr;								// �C���^�[�l�b�g�\�P�b�g�A�h���X�\����
	} SERVER_INFO_TABLE;

	// TCP��M�����\���� 
	typedef struct
	{
		void*								pReceverClass;						// ��M��N���X
		ssize_t								RecvDataSize;						// ��M�f�[�^�T�C�Y
		char*								pRecvdData;							// ��M�f�[�^�i����M��ɂăf�[�^���s�v�ƂȂ�����Afree���g�p���ė̈��������Ă��������j
	} RECV_RESPONCE_TABLE;


	CEvent*									m_pcServerDisconnectEvent;			// �T�[�o�[�ؒf�C�x���g


	CEventEx								m_cRecvResponseEvent;				// TCP��M�����C�x���g
	CMutex									m_cRecvResponseListMutex;			// TCP��M�������X�g�p�~���[�e�b�N�X
	std::list<RECV_RESPONCE_TABLE>			m_RecvResponseList;					// TCP��M�������X�g

private:
	bool									m_bInitFlag;						// �����������t���O
	int										m_ErrorNo;							// �G���[�ԍ�
	int										m_epfd;								// epoll�t�@�C���f�B�X�N���v�^�i�N���C�A���g�ڑ��Ď��X���b�h�Ŏg�p�j
	SERVER_INFO_TABLE						m_tServerInfo;						// �T�[�o�[���
	char									m_szIpAddr[IP_ADDR_BUFF_SIZE + 1];	// IP�A�h���X
	uint16_t								m_Port;								// �|�[�g�ԍ�

	char									m_szRecvBuff[CTCP_RECV_THREAD_RECV_BUFF_SIZE + 1];

public:
	CTcpRecvThread(SERVER_INFO_TABLE& tServerInfo, CEvent* pcServerDisconnectEvent);
	~CTcpRecvThread();
	int GetErrorNo();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	RESULT_ENUM GetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce);
	RESULT_ENUM SetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce);


private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void RecvResponseList_Clear();
};


