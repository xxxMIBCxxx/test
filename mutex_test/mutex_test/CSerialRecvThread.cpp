//*****************************************************************************
// �V���A���ʐM��M�X���b�h�N���X
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include "CSerialRecvThread.h"


#define _CSERIAL_RECV_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g


//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CSerialRecvThread::CSerialRecvThread()
{
	bool						bRet = false;
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;


	// �V���A����M�����C�x���g�̏�����
	eEventRet = m_cRecvResponseEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// �V���A����M�������X�g�̃N���A
	m_RecvResponseList.clear();


	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CSerialRecvThread::~CSerialRecvThread()
{
	// �V���A���ʐM��M�X���b�h��~���Y��l��
	this->Stop();

	// �ēx�A�V���A����M�������X�g���N���A����
	RecvResponseList_Clear();
}


//-----------------------------------------------------------------------------
// �V���A���ʐM��M�X���b�h�J�n
//-----------------------------------------------------------------------------
CSerialRecvThread::RESULT_ENUM CSerialRecvThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::Start - Not Init Proc.\n");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h�����삵�Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == true)
	{
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::Start - Thread Active.\n");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// �V���A���ʐM��M�X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CSerialRecvThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �V���A���ʐM��M�X���b�h��~
//-----------------------------------------------------------------------------
CSerialRecvThread::RESULT_ENUM CSerialRecvThread::Stop()
{
	bool						bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::Stop - Not Init Proc.\n");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_SUCCESS;
	}

	// �V���A���ʐM��M�X���b�h��~
	CThread::Stop();

	// �V���A����M�������X�g���N���A����
	RecvResponseList_Clear();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �V���A���ʐM��M�X���b�h
//-----------------------------------------------------------------------------
void CSerialRecvThread::ThreadProc()
{
	int							iRet = 0;
	struct epoll_event			tEvent;
	struct epoll_event			tEvents[EPOLL_MAX_EVENTS];
	bool						bLoop = true;


	// �X���b�h���I������ۂɌĂ΂��֐���o�^
	pthread_cleanup_push(ThreadProcCleanup, this);

	// epoll�t�@�C���f�B�X�N���v�^����
	m_epfd = epoll_create(EPOLL_MAX_EVENTS);
	if (m_epfd == -1)
	{
		m_ErrorNo = errno;
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		perror("CSerialRecvThread::ThreadProc - epoll_create");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return;
	}

	// �X���b�h�I���v���C�x���g��o�^
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->GetThreadEndReqEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->GetThreadEndReqEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		perror("CSerialRecvThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return;
	}

	// �V���A����M�����C�x���g��o�^
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_cRecvResponseEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_cRecvResponseEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		perror("CSerialRecvThread::ThreadProc - epoll_ctl[RecvResponseEvent]");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return;
	}

	// �X���b�h�J�n�C�x���g�𑗐M
	this->m_cThreadStartEvent.SetEvent();

	// ��--------------------------------------------------------------------------��
	// �X���b�h�I���v��������܂Ń��[�v
	// ������Ƀ��[�v�𔲂���ƃX���b�h�I�����Ƀ^�C���A�E�g�ŏI���ƂȂ邽�߁A�X���b�h�I���v���ȊO�͏���Ƀ��[�v�𔲂��Ȃ��ł�������
	while (bLoop)
	{
		memset(tEvents, 0x00, sizeof(tEvents));
		int nfds = epoll_wait(this->m_epfd, tEvents, EPOLL_MAX_EVENTS, -1);
		if (nfds == -1)
		{
			m_ErrorNo = errno;
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
			perror("CSerialRecvThread::ThreadProc - epoll_wait");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
			continue;
		}
		else if (nfds == 0)
		{
			continue;
		}

		for (int i = 0; i < nfds; i++)
		{
			// �X���b�h�I���v���C�x���g��M
			if (tEvents[i].data.fd == this->GetThreadEndReqEventFd())
			{
				bLoop = false;
				break;
			}
			// �V���A����M�����C�x���g��M
			else if (tEvents[i].data.fd == this->m_cRecvResponseEvent.GetEventFd())
			{
				// ���V���A����M����
			}
		}
	}

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// �V���A���ʐM��M�X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CSerialRecvThread::ThreadProcCleanup(void* pArg)
{
	// �p�����[�^�`�F�b�N
	if (pArg == NULL)
	{
		return;
	}
	CSerialRecvThread* pcSerialRecvThread = (CSerialRecvThread*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcSerialRecvThread->m_epfd != -1)
	{
		close(pcSerialRecvThread->m_epfd);
		pcSerialRecvThread->m_epfd = -1;
	}
}



//-----------------------------------------------------------------------------
// �V���A����M�������X�g���N���A����
//-----------------------------------------------------------------------------
void CSerialRecvThread::RecvResponseList_Clear()
{
	// ����������������������������������������������������������
	m_cRecvResponseListMutex.Lock();

	std::list<RECV_RESPONCE_TABLE>::iterator		it = m_RecvResponseList.begin();
	while (it != m_RecvResponseList.end())
	{
		RECV_RESPONCE_TABLE			tRecvResponse = *it;

		// �o�b�t�@���m�ۂ���Ă���ꍇ
		if (tRecvResponse.pRecvdData != NULL)
		{
			// �o�b�t�@�̈�����
			free(tRecvResponse.pRecvdData);
		}
		it++;
	}

	// �V���A����M�������X�g���N���A
	m_RecvResponseList.clear();

	m_cRecvResponseListMutex.Unlock();
	// ����������������������������������������������������������
}


//-----------------------------------------------------------------------------
// �V���A����M�f�[�^�擾
//-----------------------------------------------------------------------------
CSerialRecvThread::RESULT_ENUM CSerialRecvThread::GetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce)
{
	bool					bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::GetRecvResponseData - Not Init Proc.\n");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_ERROR_NOT_ACTIVE;
	}

	// ����������������������������������������������������������
	m_cRecvResponseListMutex.Lock();

	// �V���A����M�������X�g�ɓo�^�f�[�^����ꍇ
	if (m_RecvResponseList.empty() != true)
	{
		// �V���A����M�������X�g�̐擪�f�[�^�����o���i�����X�g�̐擪�f�[�^�͍폜�j
		std::list<RECV_RESPONCE_TABLE>::iterator		it = m_RecvResponseList.begin();
		tRecvResponce = *it;
		m_RecvResponseList.pop_front();
	}

	m_cRecvResponseListMutex.Unlock();
	// ����������������������������������������������������������

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �V���A����M�f�[�^�ݒ�
//-----------------------------------------------------------------------------
CSerialRecvThread::RESULT_ENUM CSerialRecvThread::SetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce)
{
	bool					bRet = false;


	// �����`�F�b�N
	if (tRecvResponce.pRecvdData == NULL)
	{
		return RESULT_ERROR_PARAM;
	}

	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::SetRecvResponseData - Not Init Proc.\n");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_ERROR_NOT_ACTIVE;
	}

	// ����������������������������������������������������������
	m_cRecvResponseListMutex.Lock();

	// �V���A����M�������X�g�ɓo�^
	m_RecvResponseList.push_back(tRecvResponce);

	m_cRecvResponseListMutex.Unlock();
	// ����������������������������������������������������������

	// �V���A����M�����C�x���g�𑗐M����
	m_cRecvResponseEvent.SetEvent();

	return RESULT_SUCCESS;
}