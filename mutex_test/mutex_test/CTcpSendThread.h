#pragma once
//*****************************************************************************
// TCP�ʐM���M�X���b�h�N���X
//*****************************************************************************
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "CThread.h"
#include "CEvent.h"
#include "CMutex.h"
#include "list"


class CTcpSendThread : CThread
{
public:
	// TCP�ʐM���M�X���b�h�N���X�̌��ʎ��
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,										// ����I��
		RESULT_ERROR_INIT = 0xE00000001,									// ���������Ɏ��s���Ă���
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,							// ���ɃX���b�h���J�n���Ă���
		RESULT_ERROR_START = 0xE00000003,									// �X���b�h�J�n�Ɏ��s���܂���

		RESULT_ERROR_NOT_ACTIVE = 0xE1000001,								// �X���b�h�����삵�Ă��Ȃ��i�܂��͏I�����Ă���j
		RESULT_ERROR_PARAM = 0xE1000002,									// �p�����[�^�G���[
		RESULT_ERROR_SYSTEM = 0xE9999999,									// �V�X�e���G���[
	} RESULT_ENUM;

	// �N���C�A���g���\����
	typedef struct
	{
		int									Socket;							// �\�P�b�g
		struct sockaddr_in					tAddr;							// �C���^�[�l�b�g�\�P�b�g�A�h���X�\����

	} CLIENT_INFO_TABLE;

	// TCP���M�v���\���� 
	typedef struct
	{
		void*								pSenderClass;					// TCP���M���N���X
		ssize_t								SendDataSize;					// TCP���M�f�[�^�T�C�Y
		char*								pSendData;						// TCP���M�f�[�^�i�����M���鑤��malloc���g�p���ė̈���m�ێw�肭�������B����͑��M�f�[�^���s�v�ƂȂ����^�C�~���O�Ŗ{�X���b�h���ŉ�����܂��j
	} SEND_REQUEST_TABLE;


	CEvent									m_cSendRequestEvent;			// TCP���M�v���C�x���g
	CMutex									m_cSendRequestListMutex;		// TCP���M�v�����X�g�p�~���[�e�b�N�X
	std::list<SEND_REQUEST_TABLE>			m_SendRequestList;				// TCP���M�v�����X�g


private:
	bool									m_bInitFlag;					// �����������t���O
	int										m_ErrorNo;						// �G���[�ԍ�
	int										m_epfd;							// epoll�t�@�C���f�B�X�N���v�^�i�N���C�A���g�ڑ��Ď��X���b�h�Ŏg�p�j
	CLIENT_INFO_TABLE						m_tClientInfo;					// �N���C�A���g���

public:
	CTcpSendThread(CLIENT_INFO_TABLE& tClientInfo);
	~CTcpSendThread();
	int GetErrorNo();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	RESULT_ENUM SetSendRequestData(SEND_REQUEST_TABLE& tSendReauest);
	RESULT_ENUM GetSendRequestData(SEND_REQUEST_TABLE& tSendReauest);

private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void SendRequestList_Clear();
};







