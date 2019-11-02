#pragma once
//*****************************************************************************
// TCP�ʐM��M�X���b�h�N���X
//*****************************************************************************
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "CThread.h"
#include "CEvent.h"
#include "CMutex.h"
#include "list"


#define CTCP_RECV_THREAD_RECV_BUFF_SIZE				( 100 )
#define CTCP_RECV_THREAD_COMMAND_BUFF_SIZE			( 1000 )


class CTcpRecvThread : public CThread
{
public:
	// TCP�ʐM��M�X���b�h�N���X�̌��ʎ��
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,										// ����I��
		RESULT_ERROR_INIT = 0xE00000001,									// ���������Ɏ��s���Ă���
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,							// ���ɃX���b�h���J�n���Ă���
		RESULT_ERROR_START = 0xE00000003,									// �X���b�h�J�n�Ɏ��s���܂���

		RESULT_ERROR_NOT_ACTIVE = 0xE1000001,								// �X���b�h�����삵�Ă��Ȃ��i�܂��͏I�����Ă���j
		RESULT_ERROR_PARAM = 0xE1000002,									// �p�����[�^�G���[
		RESULT_ERROR_RECV = 0xE1000003,										// TCP��M�G���[
		RESULT_ERROR_COMMAND_BUFF_OVER = 0xE1000004,						// �R�}���h�o�b�t�@�I�[�o�[
		RESULT_ERROR_SYSTEM = 0xE9999999,									// �V�X�e���G���[
	} RESULT_ENUM;


	// �N���C�A���g���\����
	typedef struct
	{
		int									Socket;							// �\�P�b�g
		struct sockaddr_in					tAddr;							// �C���^�[�l�b�g�\�P�b�g�A�h���X�\����

	} CLIENT_INFO_TABLE;

	// TCP��M�����\���� 
	typedef struct
	{
		void*								pReceverClass;					// ��M��N���X
		ssize_t								RecvDataSize;					// ��M�f�[�^�T�C�Y
		char*								pRecvdData;						// ��M�f�[�^�i����M��ɂăf�[�^���s�v�ƂȂ�����Afree���g�p���ė̈��������Ă��������j
	} RECV_RESPONCE_TABLE;

	// ��͎��
	typedef enum
	{
		ANALYZE_KIND_STX = 0,												// STX�҂�
		ANALYZE_KIND_ETX = 1,												// ETX�҂�
	} ANALYZE_KIND_ENUM;




	CEvent									m_cRecvResponseEvent;			// TCP��M�����C�x���g
	CMutex									m_cRecvResponseListMutex;		// TCP��M�������X�g�p�~���[�e�b�N�X
	std::list<RECV_RESPONCE_TABLE>			m_RecvResponseList;				// TCP��M�������X�g

	
	ANALYZE_KIND_ENUM						m_eAnalyzeKind;												// ��͎��
	char									m_szRecvBuff[CTCP_RECV_THREAD_RECV_BUFF_SIZE + 1];			// ��M�o�b�t�@
	char									m_szCommandBuff[CTCP_RECV_THREAD_COMMAND_BUFF_SIZE + 1];	// ��M�R�}���h�o�b�t�@
	ssize_t									m_CommandPos;												// ��M�R�}���h�i�[�ʒu

private:
	bool									m_bInitFlag;					// �����������t���O
	int										m_ErrorNo;						// �G���[�ԍ�
	int										m_epfd;							// epoll�t�@�C���f�B�X�N���v�^�i�N���C�A���g�ڑ��Ď��X���b�h�Ŏg�p�j
	CLIENT_INFO_TABLE						m_tClientInfo;					// �N���C�A���g���

public:
	CTcpRecvThread(CLIENT_INFO_TABLE &tClientInfo);
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
	RESULT_ENUM TcpRecvProc();
	RESULT_ENUM TcpRecvDataAnalyze(char* pRecvData, ssize_t RecvDataNum);
};


