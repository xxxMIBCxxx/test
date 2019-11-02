#pragma once
//*****************************************************************************
// �V���A���ʐM���M�X���b�h�N���X
//*****************************************************************************
#include "CThread.h"
#include "CEvent.h"
#include "CMutex.h"
#include "list"



class CSerialSendThread : CThread
{
public:
	// �N���C�A���g�ڑ��Ď��N���X�̌��ʎ��
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,										// ����I��
		RESULT_ERROR_INIT = 0xE00000001,									// ���������Ɏ��s���Ă���
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,							// ���ɃX���b�h���J�n���Ă���
		RESULT_ERROR_START = 0xE00000003,									// �X���b�h�J�n�Ɏ��s���܂���
		
		RESULT_ERROR_NOT_ACTIVE = 0xE1000001,								// �X���b�h�����삵�Ă��Ȃ��i�܂��͏I�����Ă���j
		RESULT_ERROR_SYSTEM = 0xE9999999,									// �V�X�e���G���[
	} RESULT_ENUM;

	// �V���A�����M�v���\���� 
	typedef struct
	{
		void*								pSenderClass;					// ���M���N���X
		ssize_t								SendDataSize;					// ���M�f�[�^�T�C�Y
		char*								pSendData;						// ���M�f�[�^�i�����M���鑤��malloc���g�p���ė̈���m�ێw�肭�������B����͑��M�f�[�^���s�v�ƂȂ����^�C�~���O�Ŗ{�X���b�h���ŉ�����܂��j
	} SEND_REQUEST_TABLE;


	CEvent									m_cSendRequestEvent;			// �V���A�����M�v���C�x���g
	CMutex									m_cSendRequestListMutex;		// �V���A�����M�v�����X�g�p�~���[�e�b�N�X
	std::list<SEND_REQUEST_TABLE>			m_SendRequestList;				// �V���A�����M�v�����X�g
	

private:
	bool									m_bInitFlag;					// �����������t���O
	int										m_ErrorNo;						// �G���[�ԍ�
	int										m_epfd;							// epoll�t�@�C���f�B�X�N���v�^�i�N���C�A���g�ڑ��Ď��X���b�h�Ŏg�p�j


public:
	CSerialSendThread();
	~CSerialSendThread();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	RESULT_ENUM SetSendRequestData(SEND_REQUEST_TABLE& tSendReauest);

private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void SendRequestList_Clear();

};







