//*****************************************************************************
// TCP�ʐM��M�X���b�h�N���X
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include "CTcpRecvThread.h"


#define _CTCP_RECV_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll�ő�C�x���g
#define STX											( 0x02 )					// STX
#define ETX											( 0x03 )					// ETX

//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CTcpRecvThread::CTcpRecvThread(SERVER_INFO_TABLE& tServerInfo, CEvent* pcServerDisconnectEvent)
{
	bool						bRet = false;
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;
	CEventEx::RESULT_ENUM		eEventExRet = CEventEx::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	m_tServerInfo = tServerInfo;
	memset(m_szIpAddr, 0x00, sizeof(m_szIpAddr));
	m_Port = 0;
	m_pcServerDisconnectEvent = NULL;

	if (pcServerDisconnectEvent == NULL)
	{
		return;
	}
	m_pcServerDisconnectEvent = pcServerDisconnectEvent;

	// �T�[�o�[�����擾
	sprintf(m_szIpAddr, "%s", inet_ntoa(m_tServerInfo.tAddr.sin_addr));			// IP�A�h���X�擾
	m_Port = ntohs(m_tServerInfo.tAddr.sin_port);								// �|�[�g�ԍ��擾

	// TCP��M�����C�x���g�̏�����
	eEventExRet = m_cRecvResponseEvent.Init();
	if (eEventExRet != CEventEx::RESULT_SUCCESS)
	{
		return;
	}

	// TCP��M�������X�g�̃N���A
	m_RecvResponseList.clear();

	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CTcpRecvThread::~CTcpRecvThread()
{
	// TCP�ʐM��M�X���b�h��~���Y��l��
	this->Stop();

	// �ēx�ATCP��M�������X�g���N���A����
	RecvResponseList_Clear();
}


//-----------------------------------------------------------------------------
// �G���[�ԍ����擾
//-----------------------------------------------------------------------------
int CTcpRecvThread::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// TCP�ʐM��M�X���b�h�J�n
//-----------------------------------------------------------------------------
CTcpRecvThread::RESULT_ENUM CTcpRecvThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_RECV_THREAD_DEBUG_
		printf("CTcpRecvThread::Start - Not Init Proc.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h�����삵�Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == true)
	{
#ifdef _CTCP_RECV_THREAD_DEBUG_
		printf("CTcpRecvThread::Start - Thread Active.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// TCP�ʐM��M�X���b�h�J�n
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CTcpRecvThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP�ʐM��M�X���b�h��~
//-----------------------------------------------------------------------------
CTcpRecvThread::RESULT_ENUM CTcpRecvThread::Stop()
{
	bool						bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_RECV_THREAD_DEBUG_
		printf("CTcpRecvThread::Stop - Not Init Proc.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// ���ɃX���b�h����~���Ă���ꍇ
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_SUCCESS;
	}

	// TCP�ʐM��M�X���b�h��~
	CThread::Stop();

	// TCP��M�������X�g���N���A����
	RecvResponseList_Clear();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP�ʐM��M�X���b�h
//-----------------------------------------------------------------------------
void CTcpRecvThread::ThreadProc()
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
#ifdef _CTCP_RECV_THREAD_DEBUG_
		perror("CTcpRecvThread::ThreadProc - epoll_create");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
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
#ifdef _CTCP_RECV_THREAD_DEBUG_
		perror("CTcpRecvThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
		return;
	}

	// TCP��M�p�̃\�P�b�g��o�^
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_tServerInfo.Socket;
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_tServerInfo.Socket, &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CTCP_RECV_THREAD_DEBUG_
		perror("CTcpRecvThread::ThreadProc - epoll_ctl[Server Socket]");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
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
#ifdef _CTCP_RECV_THREAD_DEBUG_
			perror("CTcpRecvThread::ThreadProc - epoll_wait");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
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
			// TCP��M�p�̃\�P�b�g
			else if (this->m_tServerInfo.Socket)
			{
				// TCP��M����
				memset(m_szRecvBuff, 0x00, sizeof(m_szRecvBuff));
				ssize_t read_count = read(this->m_tServerInfo.Socket, m_szRecvBuff, CTCP_RECV_THREAD_RECV_BUFF_SIZE);
				if (read_count < 0)
				{
#ifdef _CTCP_RECV_THREAD_DEBUG_
					perror("CTcpRecvThread::ThreadProc - read");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
				}
				else if (read_count == 0)
				{
					printf("[%s (%d)] - Server Disconnect.\n", m_szIpAddr, m_Port);
					// �T�[�o�[�����ؒf�����TCP��M�\�P�b�g�̒ʒm�����x������̂ŁATCP��M�\�P�b�g���폜����
					epoll_ctl(m_epfd, EPOLL_CTL_DEL, this->m_tServerInfo.Socket, NULL);
					m_pcServerDisconnectEvent->SetEvent();
				}
				else
				{
					printf("[%s (%d)] - %s\n", m_szIpAddr, m_Port, m_szRecvBuff);
				}
			}
		}
	}

	// �X���b�h�I���C�x���g�𑗐M
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// TCP�ʐM��M�X���b�h�I�����ɌĂ΂�鏈��
//-----------------------------------------------------------------------------
void CTcpRecvThread::ThreadProcCleanup(void* pArg)
{
	// �p�����[�^�`�F�b�N
	if (pArg == NULL)
	{
		return;
	}
	CTcpRecvThread* pcTcpRecvThread = (CTcpRecvThread*)pArg;


	// epoll�t�@�C���f�B�X�N���v�^���
	if (pcTcpRecvThread->m_epfd != -1)
	{
		close(pcTcpRecvThread->m_epfd);
		pcTcpRecvThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// TCP��M�������X�g���N���A����
//-----------------------------------------------------------------------------
void CTcpRecvThread::RecvResponseList_Clear()
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

	// TCP��M�������X�g���N���A
	m_RecvResponseList.clear();

	m_cRecvResponseListMutex.Unlock();
	// ����������������������������������������������������������
}


//-----------------------------------------------------------------------------
// TCP��M�f�[�^�擾
//-----------------------------------------------------------------------------
CTcpRecvThread::RESULT_ENUM CTcpRecvThread::GetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce)
{
	bool					bRet = false;


	// �������������������Ă��Ȃ��ꍇ
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_RECV_THREAD_DEBUG_
		printf("CTcpRecvThread::GetRecvResponseData - Not Init Proc.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
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

	// TCP��M�������X�g�ɓo�^�f�[�^����ꍇ
	if (m_RecvResponseList.empty() != true)
	{
		// TCP��M�������X�g�̐擪�f�[�^�����o���i�����X�g�̐擪�f�[�^�͍폜�j
		std::list<RECV_RESPONCE_TABLE>::iterator		it = m_RecvResponseList.begin();
		tRecvResponce = *it;
		m_RecvResponseList.pop_front();
	}

	m_cRecvResponseListMutex.Unlock();
	// ����������������������������������������������������������

	// TCP��M�����f�[�^���擾�����̂ŁATCP��M�����C�x���g���N���A����
	m_cRecvResponseEvent.ClearEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP��M�f�[�^�ݒ�
//-----------------------------------------------------------------------------
CTcpRecvThread::RESULT_ENUM CTcpRecvThread::SetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce)
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
#ifdef _CTCP_RECV_THREAD_DEBUG_
		printf("CTcpRecvThread::SetRecvResponseData - Not Init Proc.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
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

	// TCP��M�������X�g�ɓo�^
	m_RecvResponseList.push_back(tRecvResponce);

	m_cRecvResponseListMutex.Unlock();
	// ����������������������������������������������������������

	// TCP��M�����C�x���g�𑗐M����
	m_cRecvResponseEvent.SetEvent();

	return RESULT_SUCCESS;
}











