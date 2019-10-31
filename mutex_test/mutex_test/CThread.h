#pragma once
//*****************************************************************************
// Thread�N���X
// �������J�I�v�V�����Ɂu-pthread�v��ǉ����邱��
//*****************************************************************************
#include <cstdio>
#include <pthread.h>
#include <errno.h> 
#include <string.h>
#include "string"
#include "Type.h"
#include "CEvent.h"


// �X���b�h�̌ďo���֐���`
typedef void* (*PTHREAD_LAUNCHER_FUNC) (void*);


// CThread�N���X��`
class CThread
{
public:
	// CThread�N���X�̌��ʎ��
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,						// ����I��
		RESULT_ERROR_INIT = 0xE00000001,					// ���������Ɏ��s���Ă���
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,			// ���ɃX���b�h���J�n���Ă���
		RESULT_ERROR_START = 0xE00000003,					// �X���b�h�J�n�Ɏ��s���܂���
		RESULT_ERROR_START_TIMEOUT = 0xE0000004,			// �X���b�h�J�n�^�C���A�E�g


		RESULT_ERROR_SYSTEM = 0xE9999999					// �V�X�e���ُ�
	} RESULT_ENUM;


private:
	std::string						m_strId;				// ���ʖ�
	bool							m_bInitFlag;			// �������t���O
	int								m_iError;				// �G���[�ԍ�
	pthread_t						m_hThread;				// �X���b�h�n���h��
	pthread_mutexattr_t				m_tMutexAttr;			// �~���[�e�b�N�X����
	pthread_mutex_t					m_hMutex;				// �~���[�e�b�N�X�n���h��
	
public:	
	CEvent							m_cThreadStartEvent;	// �X���b�h�J�n�C�x���g
	CEvent							m_cThreadEndReqEvent;	// �X���b�h�I���v���C�x���g
	CEvent							m_cThreadEndEvent;		// �X���b�h�I���C�x���g


public:
	CThread(const char* pszId = NULL);
	~CThread();
	CThread::RESULT_ENUM Start();
	CThread::RESULT_ENUM Stop();
	int GetErrorNo();
	bool IsActive();
	int GetEdfThreadStartEvent();
	int GetEdfThreadEndReqEvent();
	int GetEdfThreadEndEvent();

	virtual void ThreadProc();





private:
	static void* ThreadLauncher(void* pUserData);

};

