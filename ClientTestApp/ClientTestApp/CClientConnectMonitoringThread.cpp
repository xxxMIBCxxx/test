//*****************************************************************************
// �ڑ��Ď��X���b�h�N���X�i�N���C�A���g�Łj
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
#include "CClientConnectMonitoringThread.h"


#define _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g

//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CClientConnectMonitoringThread::CClientConnectMonitoringThread(CEvent* pcServerDisconnectEvent)
{
	m_bInitFlag = false;
	m_ErrorNo = 0;
	memset(&m_tServerInfo, 0x00, sizeof(m_tServerInfo));
	memset(m_szIpAddr, 0x00, sizeof(m_szIpAddr));
	m_Port = 0;
	m_tServerInfo.Socket = -1;
	m_epfd = -1;
	m_pcTcpRecvThread = NULL;
	m_pcServerDisconnectEvent = NULL;

	if (pcServerDisconnectEvent == NULL)
	{
		return;
	}
	m_pcServerDisconnectEvent = pcServerDisconnectEvent;

	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CClientConnectMonitoringThread::~CClientConnectMonitoringThread()
{
	// �ڑ��Ď��X���b�h��~�R��l��
	this->Stop();
}


//-----------------------------------------------------------------------------
// �ڑ��Ď��X���b�h�J�n
//-----------------------------------------------------------------------------
CClientConnectMonitoringThread::RESULT_ENUM CClientConnectMonitoringThread::Start()
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

	// �T�[�o�[�ڑ�����
	eRet = ServerConnect(m_tServerInfo);
	if (eRet != RESULT_SUCCESS)
	{
		return eRet;
	}

	// �ڑ��Ď��X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();

		// �T�[�o�[�ؒf����
		ServerDisconnect(m_tServerInfo);

		return (CClientConnectMonitoringThread::RESULT_ENUM)eThreadRet;
	}

	// TCP��M�E���M�X���b�h���� & �J�n
	eRet = CreateTcpThread(m_tServerInfo);
	if (eRet != RESULT_SUCCESS)
	{
		// �ڑ��Ď��X���b�h��~
		CThread::Stop();

		// �T�[�o�[�ؒf����
		ServerDisconnect(m_tServerInfo);

		return eRet;
	}


	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �ڑ��Ď��X���b�h��~
//-----------------------------------------------------------------------------
CClientConnectMonitoringThread::RESULT_ENUM CClientConnectMonitoringThread::Stop()
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

	// TCP��M�E���M�X���b�h��� & ��~
	DeleteTcpThread();

	// �ڑ��Ď��X���b�h��~
	CThread::Stop();

	// �T�[�o�[�ؒf����
	ServerDisconnect(m_tServerInfo);

	return RESULT_SUCCESS;
}



//-----------------------------------------------------------------------------
// �ڑ��Ď��X���b�h
//-----------------------------------------------------------------------------
void CClientConnectMonitoringThread::ThreadProc()
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
#ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CClientConnectMonitoringThread::ThreadProc - epoll_create");
#endif	// #ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
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
#ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CClientConnectMonitoringThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
		return;
	}

	// �T�[�o�[�����擾
	sprintf(m_szIpAddr, "%s", inet_ntoa(m_tServerInfo.tAddr.sin_addr));			// IP�A�h���X�擾
	m_Port = ntohs(m_tServerInfo.tAddr.sin_port);								// �|�[�g�ԍ��擾

	printf("[%s (%d)] - Server Connect!\n", m_szIpAddr, m_Port);

	// �X���b�h�J�n�C�x���g�𑗐M
	this->m_cThreadStartEvent.SetEvent();


	// ��--------------------------------------------------------------------------��
	// �X���b�h�I���v��������܂Ń��[�v
	// ������Ƀ��[�v�𔲂���ƃX���b�h�I�����Ƀ^�C���A�E�g�ŏI���ƂȂ邽�߁A�X���b�h�I���v���ȊO�͏���Ƀ��[�v�𔲂��Ȃ��ł�������
	while (bLoop) {
		memset(tEvents, 0x00, sizeof(tEvents));
		int nfds = epoll_wait(this->m_epfd, tEvents, EPOLL_MAX_EVENTS, -1);
		if (nfds == -1)
		{
			m_ErrorNo = errno;
#ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
			perror("CClientConnectMonitoringThread::ThreadProc - epoll_wait");
#endif	// #ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
			continue;
		}
		// �^�C���A�E�g
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
		}
	}
	// ��--------------------------------------------------------------------------��

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// �ڑ��Ď��X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CClientConnectMonitoringThread::ThreadProcCleanup(void* pArg)
{
	CClientConnectMonitoringThread* pcClientConnectMonitoringThread = (CClientConnectMonitoringThread*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcClientConnectMonitoringThread->m_epfd != -1)
	{
		close(pcClientConnectMonitoringThread->m_epfd);
		pcClientConnectMonitoringThread->m_epfd = -1;
	}
}









//-----------------------------------------------------------------------------
// �T�[�o�[�ڑ�����
//-----------------------------------------------------------------------------
CClientConnectMonitoringThread::RESULT_ENUM CClientConnectMonitoringThread::ServerConnect(SERVER_INFO_TABLE& tServerInfo)
{
	int					iRet = 0;


	// �\�P�b�g�𐶐�
	tServerInfo.Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (tServerInfo.Socket == -1)
	{
		m_ErrorNo = errno;
#ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CClientConnectMonitoringThread::ServerConnect - socket");
#endif	// #ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
		return RESULT_ERROR_CREATE_SOCKET;
	}

	// �T�[�o�[�ڑ����ݒ�
	tServerInfo.tAddr.sin_family = AF_INET;
	tServerInfo.tAddr.sin_port = htons(12345);							// �� �|�[�g�ԍ�
	tServerInfo.tAddr.sin_addr.s_addr = inet_addr("192.168.10.9");		// �� IP�A�h���X
	if (tServerInfo.tAddr.sin_addr.s_addr == 0xFFFFFFFF)
	{
		// �z�X�g������IP�A�h���X���擾
		struct hostent* host;
		host = gethostbyname("localhost");
		tServerInfo.tAddr.sin_addr.s_addr = *(unsigned int*)host->h_addr_list[0];
	}

	// �T�[�o�[�ɐڑ�����
	iRet = connect(tServerInfo.Socket, (struct sockaddr*) & tServerInfo.tAddr, sizeof(tServerInfo.tAddr));
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CClientConnectMonitoringThread::ServerConnect - connect");
#endif	// #ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_

		// �T�[�o�[�ؒf����
		ServerDisconnect(tServerInfo);

		return RESULT_ERROR_CONNECT;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �T�[�o�[�ؒf����
//-----------------------------------------------------------------------------
void CClientConnectMonitoringThread::ServerDisconnect(SERVER_INFO_TABLE& tServerInfo)
{
	// �\�P�b�g����������Ă���ꍇ
	if (tServerInfo.Socket != -1)
	{
		close(tServerInfo.Socket);
	}

	// �T�[�o�[��񏉊���
	memset(&tServerInfo, 0x00, sizeof(tServerInfo));
	tServerInfo.Socket = -1;
}


//-----------------------------------------------------------------------------
// TCP��M�E���M�X���b�h���� & �J�n
//-----------------------------------------------------------------------------
CClientConnectMonitoringThread::RESULT_ENUM CClientConnectMonitoringThread::CreateTcpThread(SERVER_INFO_TABLE& tServerInfo)
{
	// TCP��M�X���b�h����
	CTcpRecvThread::SERVER_INFO_TABLE		tTcpRecvServerInfo;
	tTcpRecvServerInfo.Socket = m_tServerInfo.Socket;
	memcpy(&tTcpRecvServerInfo.tAddr, &m_tServerInfo.tAddr, sizeof(tTcpRecvServerInfo.tAddr));
	m_pcTcpRecvThread = (CTcpRecvThread*)new CTcpRecvThread(tTcpRecvServerInfo, m_pcServerDisconnectEvent);
	if (m_pcTcpRecvThread == NULL)
	{
		return RESULT_ERROR_SYSTEM;
	}

	// TCP��M�X���b�h�J�n
	CTcpRecvThread::RESULT_ENUM eTcpRecvThreadRet = m_pcTcpRecvThread->Start();
	if (eTcpRecvThreadRet != CTcpRecvThread::RESULT_SUCCESS)
	{
		// TCP��M�E���M�X���b�h��� & ��~
		DeleteTcpThread();

		return (CClientConnectMonitoringThread::RESULT_ENUM)eTcpRecvThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP��M�E���M�X���b�h��� & ��~
//-----------------------------------------------------------------------------
void CClientConnectMonitoringThread::DeleteTcpThread()
{
	// TCP��M�X���b�h���
	if (m_pcTcpRecvThread != NULL)
	{
		delete m_pcTcpRecvThread;
		m_pcTcpRecvThread = NULL;
	}
}
