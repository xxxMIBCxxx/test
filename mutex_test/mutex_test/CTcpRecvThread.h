#pragma once
//*****************************************************************************
// TCP�ʐM��M�X���b�h�N���X
//*****************************************************************************
#include "CThread.h"
#include "CEvent.h"
#include "CMutex.h"
#include "list"


class CTcpRecvThread : public CThread
{
	// �V���A���ʐM��M�X���b�h�N���X�̌��ʎ��
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

	// �V���A����M�����\���� 
	typedef struct
	{
		void*								pReceverClass;					// ��M��N���X
		ssize_t								RecvDataSize;					// ��M�f�[�^�T�C�Y
		char*								pRecvdData;						// ��M�f�[�^�i����M��ɂăf�[�^���s�v�ƂȂ�����Afree���g�p���ė̈��������Ă��������j
	} RECV_RESPONCE_TABLE;


	CEvent									m_cRecvResponseEvent;			// TCP��M�����C�x���g
	CMutex									m_cRecvResponseListMutex;		// TCP��M�������X�g�p�~���[�e�b�N�X
	std::list<RECV_RESPONCE_TABLE>			m_RecvResponseList;				// TCP��M�������X�g


private:
	bool									m_bInitFlag;					// �����������t���O
	int										m_ErrorNo;						// �G���[�ԍ�
	int										m_epfd;							// epoll�t�@�C���f�B�X�N���v�^�i�N���C�A���g�ڑ��Ď��X���b�h�Ŏg�p�j

public:
	CTcpRecvThread();
	~CTcpRecvThread();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	RESULT_ENUM GetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce);
	RESULT_ENUM SetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce);

private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void RecvResponseList_Clear();
};


