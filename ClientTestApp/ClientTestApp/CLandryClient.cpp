//*****************************************************************************
// LandryClient
//*****************************************************************************
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <memory.h>
#include <unistd.h>
#include <sys/epoll.h>
#include "CLandryClient.h"


#define _CLANDRY_CLIENT_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g
#define EPOLL_TIMEOUT_TIME							( 1000 * 5 )				// epoll�^�C���A�E�g����(ms)


//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CLandryClient::CLandryClient()
{
	CEvent::RESULT_ENUM  eEventRet = CEvent::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	m_pcClinentConnectMonitoringThread = NULL;

	// �T�[�o�[�ؒf�C�x���g�̏�����
	eEventRet = m_cServerDisconnectEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CLandryClient::~CLandryClient()
{	
	// LandryClient�X���b�h��~�R����l��
	this->Stop();
}


//-----------------------------------------------------------------------------
// LandryClient�X���b�h�J�n
//-----------------------------------------------------------------------------
CLandryClient::RESULT_ENUM CLandryClient::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h�����삵�Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == true)
	{
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// LandryClient�X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CLandryClient::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// LandryClient�X���b�h��~
//-----------------------------------------------------------------------------
CLandryClient::RESULT_ENUM CLandryClient::Stop()
{
	bool						bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_SUCCESS;
	}

	// �ڑ��Ď��X���b�h�i�N���C�A���g�Łj���
	DeleteClientMonitoringThread();

	// LandryClient�X���b�h��~
	CThread::Stop();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// LandryClient�X���b�h
//-----------------------------------------------------------------------------
void CLandryClient::ThreadProc()
{
	int							iRet = 0;
	struct epoll_event			tEvent;
	struct epoll_event			tEvents[EPOLL_MAX_EVENTS];
	bool						bLoop = true;
	ssize_t						ReadNum = 0;


	// �X���b�h���I������ۂɌĂ΂��֐���o�^
	pthread_cleanup_push(ThreadProcCleanup, this);

	// epoll�t�@�C���f�B�X�N���v�^����
	m_epfd = epoll_create(EPOLL_MAX_EVENTS);
	if (m_epfd == -1)
	{
		m_ErrorNo = errno;
#ifdef _CLANDRY_CLIENT_DEBUG_
		perror("CLandryClient::ThreadProc - epoll_create");
#endif	// #ifdef _CLANDRY_CLIENT_DEBUG_
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
#ifdef _CLANDRY_CLIENT_DEBUG_
		perror("CLandryClient::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CLANDRY_CLIENT_DEBUG_
		return;
	}

	// �T�[�o�[�ؒf�C�x���g��o�^
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_cServerDisconnectEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_cServerDisconnectEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CLANDRY_CLIENT_DEBUG_
		perror("CLandryClient::ThreadProc - epoll_ctl[ServerDisconnectEvent]");
#endif	// #ifdef _CLANDRY_CLIENT_DEBUG_
		return;
	}

	// �ڑ��Ď��X���b�h�i�N���C�A���g�Łj�̐���
	CreateClientMonitoringThread();

	// �X���b�h�J�n�C�x���g�𑗐M
	this->m_cThreadStartEvent.SetEvent();


	// ��--------------------------------------------------------------------------��
	// �X���b�h�I���v��������܂Ń��[�v
	// ������Ƀ��[�v�𔲂���ƃX���b�h�I�����Ƀ^�C���A�E�g�ŏI���ƂȂ邽�߁A�X���b�h�I���v���ȊO�͏���Ƀ��[�v�𔲂��Ȃ��ł�������
	while (bLoop) {
		memset(tEvents, 0x00, sizeof(tEvents));
		int nfds = epoll_wait(this->m_epfd, tEvents, EPOLL_MAX_EVENTS, EPOLL_TIMEOUT_TIME);
		if (nfds == -1)
		{
			m_ErrorNo = errno;
#ifdef _CLANDRY_CLIENT_DEBUG_
			perror("CLandryClient::ThreadProc - epoll_wait");
#endif	// #ifdef _CLANDRY_CLIENT_DEBUG_
			continue;
		}
		// �^�C���A�E�g
		else if (nfds == 0)
		{
			// �T�[�o�[�Ɛڑ����Ă��Ȃ��ꍇ�A�ڑ������݂�
			if (m_pcClinentConnectMonitoringThread == NULL)
			{
				// �ڑ��Ď��X���b�h�i�N���C�A���g�Łj�̐���
				CreateClientMonitoringThread();
			}
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
			// �T�[�o�[�ؒf�C�x���g
			else if (tEvents[i].data.fd = this->m_cServerDisconnectEvent.GetEventFd())
			{
				m_cServerDisconnectEvent.ClearEvent();
				// �ڑ��Ď��X���b�h�i�N���C�A���g�Łj���
				DeleteClientMonitoringThread();
			}
		}
	}
	// ��--------------------------------------------------------------------------��

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// LandryClient�X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CLandryClient::ThreadProcCleanup(void* pArg)
{
	CLandryClient* pcLandryClient = (CLandryClient*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcLandryClient->m_epfd != -1)
	{
		close(pcLandryClient->m_epfd);
		pcLandryClient->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// �ڑ��Ď��X���b�h�i�N���C�A���g�Łj����
//-----------------------------------------------------------------------------
CLandryClient::RESULT_ENUM CLandryClient::CreateClientMonitoringThread()
{
	// ���ɐ������Ă���ꍇ
	if (m_pcClinentConnectMonitoringThread != NULL)
	{
		return RESULT_SUCCESS;
	}

	// �ڑ��Ď��X���b�h�i�N���C�A���g�Łj�̐���
	m_pcClinentConnectMonitoringThread = (CClientConnectMonitoringThread*)new CClientConnectMonitoringThread(&m_cServerDisconnectEvent);
	if (m_pcClinentConnectMonitoringThread == NULL)
	{
#ifdef _CLANDRY_CLIENT_DEBUG_
		printf("CLandryClient::CreateClientMonitoringThread - Create ClientConnectMonitoringThread Error.\n");
#endif	// #ifdef _CLANDRY_CLIENT_DEBUG_
		return RESULT_ERROR_SYSTEM;
	}

	// �ڑ��Ď��X���b�h�i�N���C�A���g�Łj�J�n
	CClientConnectMonitoringThread::RESULT_ENUM	eRet = m_pcClinentConnectMonitoringThread->Start();
	if (eRet != CClientConnectMonitoringThread::RESULT_SUCCESS)
	{
#ifdef _CLANDRY_CLIENT_DEBUG_
//		printf("CLandryClient::CreateClientMonitoringThread - Start ClientConnectMonitoringThread Error.\n");
#endif	// #ifdef _CLANDRY_CLIENT_DEBUG_

		// �ڑ��Ď��X���b�h�i�N���C�A���g�Łj���
		DeleteClientMonitoringThread();

		return (CLandryClient::RESULT_ENUM)eRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �ڑ��Ď��X���b�h�i�N���C�A���g�Łj���
//-----------------------------------------------------------------------------
void CLandryClient::DeleteClientMonitoringThread()
{
	// �ڑ��Ď��X���b�h�i�N���C�A���g�Łj�̔j��
	if (m_pcClinentConnectMonitoringThread != NULL)
	{
		delete m_pcClinentConnectMonitoringThread;
		m_pcClinentConnectMonitoringThread = NULL;
	}
}

