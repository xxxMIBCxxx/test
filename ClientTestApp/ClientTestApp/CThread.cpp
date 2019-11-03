//*****************************************************************************
// Thread�N���X
// �������J�I�v�V�����Ɂu-pthread�v��ǉ����邱��
//*****************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "CThread.h"


#define _CTHREAD_DEBUG_
#define	CTHREAD_START_TIMEOUT				( 3 * 1000 )			// �X���b�h�J�n�҂��^�C���A�E�g(ms)
#define	CTHREAD_END_TIMEOUT					( 3 * 1000 )			// �X���b�h�I���҂��^�C���A�E�g(ms)


//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CThread::CThread(const char* pszId)
{
	int							iRet = 0;
	CEvent::RESULT_ENUM			eRet = CEvent::RESULT_SUCCESS;


	m_strId = "";
	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_hThread = 0;
	
	// �N���X����ێ�
	if (pszId != NULL)
	{
		m_strId = pszId;
	}

	// �X���b�h�J�n�C�x���g������
	eRet = m_cThreadStartEvent.Init();
	if (eRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// �X���b�h�I���v���C�x���g
	eRet = m_cThreadEndReqEvent.Init();
	if (eRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// �X���b�h�I���p�C�x���g������
	eRet = m_cThreadEndEvent.Init();
	if (eRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CThread::~CThread()
{
	// �X���b�h����~���Ă��Ȃ����Ƃ��l��
	this->Stop();
}


//-----------------------------------------------------------------------------
// �X���b�h�J�n
//-----------------------------------------------------------------------------
CThread::RESULT_ENUM CThread::Start()
{
	int						iRet = 0;
	CEvent::RESULT_ENUM		eEventRet = CEvent::RESULT_SUCCESS;


	// �����������Ŏ��s���Ă���ꍇ
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// ���ɓ��삵�Ă���ꍇ
	if (m_hThread != 0)
	{
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// �X���b�h�J�n
	this->m_cThreadStartEvent.ClearEvent();
	this->m_cThreadEndReqEvent.ClearEvent();
	iRet = pthread_create(&m_hThread, NULL, ThreadLauncher, this);
	if (iRet != 0)
	{
		m_ErrorNo = errno;
#ifdef _CTHREAD_DEBUG_
		perror("CThread::Start - pthread_create");
#endif	// #ifdef _CTHREAD_DEBUG_
		return RESULT_ERROR_START;
	}

	// �X���b�h�J�n�C�x���g�҂�
	eEventRet = this->m_cThreadStartEvent.Wait(CTHREAD_START_TIMEOUT);
	switch (eEventRet) {
	case CEvent::RESULT_RECIVE_EVENT:			// �X���b�h�J�n�C�x���g����M
		this->m_cThreadStartEvent.ClearEvent();
		break;

	case CEvent::RESULT_WAIT_TIMEOUT:			// �^�C���A�E�g
#ifdef _CTHREAD_DEBUG_
		printf("CThread::Start - WaitTimeout\n");
#endif	// #ifdef _CTHREAD_DEBUG_
		pthread_cancel(m_hThread);
		pthread_join(m_hThread, NULL);
		m_hThread = 0;
		return RESULT_ERROR_START_TIMEOUT;

	default:
#ifdef _CTHREAD_DEBUG_
		printf("CThread::Start - Wait Error. [0x%08X]\n", eEventRet);
#endif	// #ifdef _CTHREAD_DEBUG_
		pthread_cancel(m_hThread);
		pthread_join(m_hThread, NULL);
		m_hThread = 0;
		return RESULT_ERROR_SYSTEM;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �X���b�h��~
//-----------------------------------------------------------------------------
CThread::RESULT_ENUM CThread::Stop()
{
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;


	// �����������Ŏ��s���Ă���ꍇ
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// ���ɒ�~���Ă���ꍇ
	if (m_hThread == 0)
	{
		return RESULT_SUCCESS;
	}

	// �X���b�h���~������i�X���b�h�I���v���C�x���g�𑗐M�j
	this->m_cThreadEndEvent.ClearEvent();
	eEventRet = this->m_cThreadEndReqEvent.SetEvent();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
#ifdef _CTHREAD_DEBUG_
		printf("CThread::Stop - SetEvent Error. [0x%08X]\n", eEventRet);
#endif	// #ifdef _CTHREAD_DEBUG_
		
		// �X���b�h��~�Ɏ��s�����ꍇ�́A�����I�ɏI��������
		pthread_cancel(m_hThread);
	}
	else
	{
		// �X���b�h�I���C�x���g�҂�
		eEventRet = this->m_cThreadEndEvent.Wait(CTHREAD_END_TIMEOUT);
		switch (eEventRet) {
		case CEvent::RESULT_RECIVE_EVENT:			// �X���b�h�I���C�x���g����M
			this->m_cThreadEndEvent.ClearEvent();
			break;

		case CEvent::RESULT_WAIT_TIMEOUT:			// �^�C���A�E�g
#ifdef _CTHREAD_DEBUG_
			printf("CThread::Stop - Timeout\n");
#endif	// #ifdef _CTHREAD_DEBUG_
			pthread_cancel(m_hThread);
			break;

		default:
#ifdef _CTHREAD_DEBUG_
			printf("CThread::Stop - Wait Error. [0x%08X]\n", eEventRet);
#endif	// #ifdef _CTHREAD_DEBUG_
			pthread_cancel(m_hThread);
			break;
		}
	}
	pthread_join(m_hThread, NULL);
	m_hThread = 0;

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �G���[�ԍ����擾
//-----------------------------------------------------------------------------
int CThread::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// �X���b�h�����삵�Ă��邩�̊m�F
//-----------------------------------------------------------------------------
bool CThread::IsActive()
{
	return ((m_hThread != 0) ? true : false);
}


//-----------------------------------------------------------------------------
// �X���b�h�J�n�C�x���g�t�@�C���f�B�X�N���v�^���擾
//-----------------------------------------------------------------------------
int CThread::GetThreadStartEventFd()
{
	return m_cThreadStartEvent.GetEventFd();
}


//-----------------------------------------------------------------------------
// �X���b�h�I���v���C�x���g�t�@�C���f�B�X�N���v�^���擾
//-----------------------------------------------------------------------------
int CThread::GetThreadEndReqEventFd()
{
	return m_cThreadEndReqEvent.GetEventFd();
}


//-----------------------------------------------------------------------------
// �X���b�h�I���C�x���g�t�@�C���f�B�X�N���v�^���擾
//-----------------------------------------------------------------------------
int CThread::GetThreadEndEventFd()
{
	return m_cThreadEndEvent.GetEventFd();
}


//-----------------------------------------------------------------------------
// �X���b�h�Ăяo��
//-----------------------------------------------------------------------------
void* CThread::ThreadLauncher(void* pUserData)
{
	// �X���b�h�����Ăяo��
	reinterpret_cast<CThread*>(pUserData)->ThreadProc();

	return (void *)NULL;
}


//-----------------------------------------------------------------------------
// �X���b�h�����i���T���v�����j
//-----------------------------------------------------------------------------
void CThread::ThreadProc()
{
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;
	unsigned int				Count = 1;
	unsigned int				Timeout = 0;
	bool						bLoop = true;


	// �X���b�h�J�n�C�x���g�𑗐M
	m_cThreadStartEvent.SetEvent();
	printf("-- Thread %s Start --\n", m_strId.c_str());

	// pthread_testcancel���Ă΂��܂ŏ����𑱂���
	while (bLoop)
	{
		// Sleep���Ԃ𐶐�
		Timeout = ((rand() % 30) + 1) * 100;

		// �X���b�h�I���v���C�x���g���ʒm�����܂ő҂�
		eEventRet = m_cThreadEndReqEvent.Wait(Timeout);
		switch (eEventRet) {
		case CEvent::RESULT_WAIT_TIMEOUT:
			printf("[%s] Count : %lu \n", this->m_strId.c_str(), Count++);
			break;

		case CEvent::RESULT_RECIVE_EVENT:
			m_cThreadEndReqEvent.ClearEvent();
			bLoop = false;
			break;

		default:
			printf("CEvent::Wait error. \n");
			bLoop = false;
			break;
		}
	}

	// �X���b�h�I���C�x���g�𑗐M
	m_cThreadEndEvent.SetEvent();
	printf("-- Thread %s End --\n", m_strId.c_str());
}
