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
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント
#define EPOLL_TIMEOUT_TIME							( 1000 * 5 )				// epollタイムアウト時間(ms)


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CLandryClient::CLandryClient()
{
	CEvent::RESULT_ENUM  eEventRet = CEvent::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	m_pcClinentConnectMonitoringThread = NULL;

	// サーバー切断イベントの初期化
	eEventRet = m_cServerDisconnectEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CLandryClient::~CLandryClient()
{	
	// LandryClientスレッド停止漏れを考慮
	this->Stop();
}


//-----------------------------------------------------------------------------
// LandryClientスレッド開始
//-----------------------------------------------------------------------------
CLandryClient::RESULT_ENUM CLandryClient::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが動作している場合
	bRet = this->IsActive();
	if (bRet == true)
	{
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// LandryClientスレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CLandryClient::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// LandryClientスレッド停止
//-----------------------------------------------------------------------------
CLandryClient::RESULT_ENUM CLandryClient::Stop()
{
	bool						bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_SUCCESS;
	}

	// 接続監視スレッド（クライアント版）解放
	DeleteClientMonitoringThread();

	// LandryClientスレッド停止
	CThread::Stop();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// LandryClientスレッド
//-----------------------------------------------------------------------------
void CLandryClient::ThreadProc()
{
	int							iRet = 0;
	struct epoll_event			tEvent;
	struct epoll_event			tEvents[EPOLL_MAX_EVENTS];
	bool						bLoop = true;
	ssize_t						ReadNum = 0;


	// スレッドが終了する際に呼ばれる関数を登録
	pthread_cleanup_push(ThreadProcCleanup, this);

	// epollファイルディスクリプタ生成
	m_epfd = epoll_create(EPOLL_MAX_EVENTS);
	if (m_epfd == -1)
	{
		m_ErrorNo = errno;
#ifdef _CLANDRY_CLIENT_DEBUG_
		perror("CLandryClient::ThreadProc - epoll_create");
#endif	// #ifdef _CLANDRY_CLIENT_DEBUG_
		return;
	}

	// スレッド終了要求イベントを登録
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

	// サーバー切断イベントを登録
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

	// 接続監視スレッド（クライアント版）の生成
	CreateClientMonitoringThread();

	// スレッド開始イベントを送信
	this->m_cThreadStartEvent.SetEvent();


	// ▼--------------------------------------------------------------------------▼
	// スレッド終了要求が来るまでループ
	// ※勝手にループを抜けるとスレッド終了時にタイムアウトで終了となるため、スレッド終了要求以外は勝手にループを抜けないでください
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
		// タイムアウト
		else if (nfds == 0)
		{
			// サーバーと接続していない場合、接続を試みる
			if (m_pcClinentConnectMonitoringThread == NULL)
			{
				// 接続監視スレッド（クライアント版）の生成
				CreateClientMonitoringThread();
			}
			continue;
		}

		for (int i = 0; i < nfds; i++)
		{
			// スレッド終了要求イベント受信
			if (tEvents[i].data.fd == this->GetThreadEndReqEventFd())
			{
				bLoop = false;
				break;
			}
			// サーバー切断イベント
			else if (tEvents[i].data.fd = this->m_cServerDisconnectEvent.GetEventFd())
			{
				m_cServerDisconnectEvent.ClearEvent();
				// 接続監視スレッド（クライアント版）解放
				DeleteClientMonitoringThread();
			}
		}
	}
	// ▲--------------------------------------------------------------------------▲

	// スレッド終了イベントを送信
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// LandryClientスレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CLandryClient::ThreadProcCleanup(void* pArg)
{
	CLandryClient* pcLandryClient = (CLandryClient*)pArg;


	// epollファイルディスクリプタ解放
	if (pcLandryClient->m_epfd != -1)
	{
		close(pcLandryClient->m_epfd);
		pcLandryClient->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// 接続監視スレッド（クライアント版）生成
//-----------------------------------------------------------------------------
CLandryClient::RESULT_ENUM CLandryClient::CreateClientMonitoringThread()
{
	// 既に生成している場合
	if (m_pcClinentConnectMonitoringThread != NULL)
	{
		return RESULT_SUCCESS;
	}

	// 接続監視スレッド（クライアント版）の生成
	m_pcClinentConnectMonitoringThread = (CClientConnectMonitoringThread*)new CClientConnectMonitoringThread(&m_cServerDisconnectEvent);
	if (m_pcClinentConnectMonitoringThread == NULL)
	{
#ifdef _CLANDRY_CLIENT_DEBUG_
		printf("CLandryClient::CreateClientMonitoringThread - Create ClientConnectMonitoringThread Error.\n");
#endif	// #ifdef _CLANDRY_CLIENT_DEBUG_
		return RESULT_ERROR_SYSTEM;
	}

	// 接続監視スレッド（クライアント版）開始
	CClientConnectMonitoringThread::RESULT_ENUM	eRet = m_pcClinentConnectMonitoringThread->Start();
	if (eRet != CClientConnectMonitoringThread::RESULT_SUCCESS)
	{
#ifdef _CLANDRY_CLIENT_DEBUG_
//		printf("CLandryClient::CreateClientMonitoringThread - Start ClientConnectMonitoringThread Error.\n");
#endif	// #ifdef _CLANDRY_CLIENT_DEBUG_

		// 接続監視スレッド（クライアント版）解放
		DeleteClientMonitoringThread();

		return (CLandryClient::RESULT_ENUM)eRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// 接続監視スレッド（クライアント版）解放
//-----------------------------------------------------------------------------
void CLandryClient::DeleteClientMonitoringThread()
{
	// 接続監視スレッド（クライアント版）の破棄
	if (m_pcClinentConnectMonitoringThread != NULL)
	{
		delete m_pcClinentConnectMonitoringThread;
		m_pcClinentConnectMonitoringThread = NULL;
	}
}

