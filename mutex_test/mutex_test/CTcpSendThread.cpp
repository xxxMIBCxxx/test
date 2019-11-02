//*****************************************************************************
// TCP�ʐM���M�X���b�h�N���X
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include "CTcpSendThread.h"


#define _CTCP_SEND_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g



//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CTcpSendThread::CTcpSendThread()
{
	bool						bRet = false;
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;


	// TCP���M�v���C�x���g�̏�����
	eEventRet = m_cSendRequestEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// TCP���M�v�����X�g�̃N���A
	m_SendRequestList.clear();


	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CTcpSendThread::~CTcpSendThread()
{
	// TCP�ʐM���M�X���b�h��~���Y��l��
	this->Stop();

	// �ēx�ATCP���M�v�����X�g���N���A����
	SendRequestList_Clear();
}


//-----------------------------------------------------------------------------
// TCP�ʐM���M�X���b�h�J�n
//-----------------------------------------------------------------------------
CTcpSendThread::RESULT_ENUM CTcpSendThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_SEND_THREAD_DEBUG_
		printf("CTcpSendThread::Start - Not Init Proc.\n");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h�����삵�Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == true)
	{
#ifdef _CTCP_SEND_THREAD_DEBUG_
		printf("CTcpSendThread::Start - Thread Active.\n");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// TCP�ʐM���M�X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CTcpSendThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP�ʐM���M�X���b�h��~
//-----------------------------------------------------------------------------
CTcpSendThread::RESULT_ENUM CTcpSendThread::Stop()
{
	bool						bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_SEND_THREAD_DEBUG_
		printf("CTcpSendThread::Stop - Not Init Proc.\n");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_SUCCESS;
	}

	// TCP�ʐM���M�X���b�h��~
	CThread::Stop();

	// TCP���M�v�����X�g���N���A����
	SendRequestList_Clear();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP�ʐM���M�X���b�h
//-----------------------------------------------------------------------------
void CTcpSendThread::ThreadProc()
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
#ifdef _CTCP_SEND_THREAD_DEBUG_
		perror("CTcpSendThread::ThreadProc - epoll_create");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
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
#ifdef _CTCP_SEND_THREAD_DEBUG_
		perror("CTcpSendThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return;
	}

	// TCP���M�v���C�x���g��o�^
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_cSendRequestEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_cSendRequestEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CTCP_SEND_THREAD_DEBUG_
		perror("CTcpSendThread::ThreadProc - epoll_ctl[SendRequestEvent]");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
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
#ifdef _CTCP_SEND_THREAD_DEBUG_
			perror("CTcpSendThread::ThreadProc - epoll_wait");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
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
			// TCP���M�v���C�x���g��M
			else if (tEvents[i].data.fd == this->m_cSendRequestEvent.GetEventFd())
			{
				// TCP���M����
			}
		}
	}

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// TCP�ʐM���M�X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CTcpSendThread::ThreadProcCleanup(void* pArg)
{
	// �p�����[�^�`�F�b�N
	if (pArg == NULL)
	{
		return;
	}
	CTcpSendThread* pcTcpSendThread = (CTcpSendThread*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcTcpSendThread->m_epfd != -1)
	{
		close(pcTcpSendThread->m_epfd);
		pcTcpSendThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// TCP���M�v�����X�g���N���A����
//-----------------------------------------------------------------------------
void CTcpSendThread::SendRequestList_Clear()
{
	// ����������������������������������������������������������
	m_cSendRequestListMutex.Lock();

	std::list<SEND_REQUEST_TABLE>::iterator		it = m_SendRequestList.begin();
	while (it != m_SendRequestList.end())
	{
		SEND_REQUEST_TABLE			tSendRequest = *it;

		// �o�b�t�@���m�ۂ���Ă���ꍇ
		if (tSendRequest.pSendData != NULL)
		{
			// �o�b�t�@�̈�����
			free(tSendRequest.pSendData);
		}
		it++;
	}

	// �V���A�����M�v�����X�g���N���A
	m_SendRequestList.clear();

	m_cSendRequestListMutex.Unlock();
	// ����������������������������������������������������������
}


//-----------------------------------------------------------------------------
// TCP���M�v���f�[�^�ݒ�
//-----------------------------------------------------------------------------
CTcpSendThread::RESULT_ENUM CTcpSendThread::SetSendRequestData(SEND_REQUEST_TABLE& tSendReauest)
{
	bool					bRet = false;


	// �����`�F�b�N
	if (tSendReauest.pSendData == NULL)
	{
		return RESULT_ERROR_PARAM;
	}

	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_SEND_THREAD_DEBUG_
		printf("CTcpSendThread::SetSendRequestData - Not Init Proc.\n");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_ERROR_NOT_ACTIVE;
	}

	// ����������������������������������������������������������
	m_cSendRequestListMutex.Lock();

	// TCP���M�v�����X�g�ɓo�^
	m_SendRequestList.push_back(tSendReauest);

	m_cSendRequestListMutex.Unlock();
	// ����������������������������������������������������������

	// TCP�ʐM���M�X���b�h�ɃV���A�����M�v���C�x���g�𑗐M����
	m_cSendRequestEvent.SetEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP���M�v���f�[�^�擾
//-----------------------------------------------------------------------------
CTcpSendThread::RESULT_ENUM CTcpSendThread::GetSendRequestData(SEND_REQUEST_TABLE& tSendReauest)
{
	bool					bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_SEND_THREAD_DEBUG_
		printf("CTcpSendThread::GetSendRequestData - Not Init Proc.\n");
#endif	// #ifdef _CTCP_SEND_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_ERROR_NOT_ACTIVE;
	}

	// ����������������������������������������������������������
	m_cSendRequestListMutex.Lock();

	// TCP���M�v�����X�g�ɓo�^�f�[�^����ꍇ
	if (m_SendRequestList.empty() != true)
	{
		// TCP��M�������X�g�̐擪�f�[�^�����o���i�����X�g�̐擪�f�[�^�͍폜�j
		std::list<SEND_REQUEST_TABLE>::iterator		it = m_SendRequestList.begin();
		tSendReauest = *it;
		m_SendRequestList.pop_front();
	}

	m_cSendRequestListMutex.Unlock();
	// ����������������������������������������������������������

	// TCP�ʐM���M�X���b�h�ɃV���A�����M�v���C�x���g�𑗐M����
	m_cSendRequestEvent.SetEvent();

	return RESULT_SUCCESS;
}