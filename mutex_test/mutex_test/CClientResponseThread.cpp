//*****************************************************************************
// クライアント応答スレッドクラス
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include "CClientResponseThread.h"


#define _CCLIENT_RESPONSE_THREAD_DEBUG_
#define CLIENT_CONNECT_NUM							( 5 )						// クライアント接続可能数
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CClientResponseThread::CClientResponseThread(CLIENT_INFO_TABLE& tClientInfo, CEvent* pcClientResponseThread_EndEvent)
{
	m_bInitFlag = false;
	m_ErrorNo = 0;
	memset(&m_tClientInfo, 0x00, sizeof(m_tClientInfo));
	memset(m_szIpAddr, 0x00, sizeof(m_szIpAddr));
	m_Port = 0;
	m_epfd = -1;
	memset(m_szRecvBuf, 0x00, sizeof(m_szRecvBuf));
	m_bClientResponseThread_EndFlag = false;
	m_pcClientResponseThread_EndEvent = NULL;
	m_pcTcpSendThread = NULL;
	m_pcTcpRecvThread = NULL;


	// クライアント終了イベントのチェック
	if (pcClientResponseThread_EndEvent == NULL)
	{
		return;
	}
	m_pcClientResponseThread_EndEvent = pcClientResponseThread_EndEvent;

	// クライアント情報を取得
	memcpy(&m_tClientInfo, &tClientInfo, sizeof(CLIENT_INFO_TABLE));
	sprintf(m_szIpAddr, "%s", inet_ntoa(m_tClientInfo.tAddr.sin_addr));			// IPアドレス取得
	m_Port = ntohs(m_tClientInfo.tAddr.sin_port);								// ポート番号取得

	//// TCP送信スレッドクラスを生成
	//CTcpSendThread::CLIENT_INFO_TABLE		tTcpSendClientInfo;
	//tTcpSendClientInfo.Socket = tClientInfo.Socket;
	//tTcpSendClientInfo.tAddr = tClientInfo.tAddr;
	//m_pcTcpSendThread = (CTcpSendThread*)new CTcpSendThread(tTcpSendClientInfo);
	//if (m_pcTcpSendThread == NULL)
	//{
	//	return;
	//}

	// TCP受信スレッドクラスを生成
	CTcpRecvThread::CLIENT_INFO_TABLE		tTcpRecvClientInfo;
	tTcpRecvClientInfo.Socket = tClientInfo.Socket;
	tTcpRecvClientInfo.tAddr = tClientInfo.tAddr;
	m_pcTcpRecvThread = (CTcpRecvThread*)new CTcpRecvThread(tTcpRecvClientInfo);
	if (m_pcTcpRecvThread == NULL)
	{
		return;
	}

	// 初期化完了
	m_bInitFlag = true;
}



//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CClientResponseThread::~CClientResponseThread()
{
	// クライアント応答スレッド終了漏れを考慮
	this->Stop();

	//// TCP送信スレッドクラスを破棄
	//if (m_pcTcpSendThread != NULL)
	//{
	//	delete m_pcTcpSendThread;
	//	m_pcTcpSendThread = NULL;
	//}

	// TCP受信スレッドクラスを破棄
	if (m_pcTcpRecvThread != NULL)
	{
		delete m_pcTcpRecvThread;
		m_pcTcpRecvThread = NULL;
	}

	// クライアント側のソケットを解放
	if (m_tClientInfo.Socket != -1)
	{
		close(m_tClientInfo.Socket);
		m_tClientInfo.Socket = -1;
	}
}



//-----------------------------------------------------------------------------
// クライアント応答スレッド開始
//-----------------------------------------------------------------------------
CClientResponseThread::RESULT_ENUM CClientResponseThread::Start()
{
	bool						bRet = false;
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

	//// TCP送信スレッド開始
	//CTcpSendThread::RESULT_ENUM eTcpSendThreadResult = m_pcTcpSendThread->Start();
	//if (eTcpSendThreadResult != CTcpSendThread::RESULT_SUCCESS)
	//{
	//	m_ErrorNo = m_pcTcpSendThread->GetErrorNo();
	//	return (CClientResponseThread::RESULT_ENUM)eTcpSendThreadResult;
	//}

	// TCP受信スレッド開始
	CTcpRecvThread::RESULT_ENUM eTcpRecvThreadResult = m_pcTcpRecvThread->Start();
	if (eTcpRecvThreadResult != CTcpRecvThread::RESULT_SUCCESS)
	{
		m_ErrorNo = m_pcTcpRecvThread->GetErrorNo();
		return (CClientResponseThread::RESULT_ENUM)eTcpRecvThreadResult;
	}

	// クライアント接続監視スレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CClientResponseThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// クライアント応答スレッド停止
//-----------------------------------------------------------------------------
CClientResponseThread::RESULT_ENUM CClientResponseThread::Stop()
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

	//// TCP送信スレッド停止
	//m_pcTcpSendThread->Stop();

	// TCP受信スレッド停止
	m_pcTcpRecvThread->Stop();

	// クライアント接続監視スレッド停止
	CThread::Stop();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// クライアント応答スレッド
//-----------------------------------------------------------------------------
void CClientResponseThread::ThreadProc()
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
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		perror("CClientResponseThread - epoll_create");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
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
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		perror("CClientResponseThread - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		return;
	}

	// クライアント切断イベントを登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_pcTcpRecvThread->m_cClientDisconnectEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_pcTcpRecvThread->m_cClientDisconnectEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		perror("CClientResponseThread - epoll_ctl[ClientDisconnectEvent]");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		return;
	}

//	// TCP送信要求イベントを登録
//	memset(&tEvent, 0x00, sizeof(tEvent));
//	tEvent.events = EPOLLIN;
//	tEvent.data.fd = this->m_pcTcpSendThread->m_cSendRequestEvent.GetEventFd();
//	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_pcTcpSendThread->m_cSendRequestEvent.GetEventFd(), &tEvent);
//	if (iRet == -1)
//	{
//		m_ErrorNo = errno;
//#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
//		perror("CClientResponseThread - epoll_ctl[TcpSendRequestEvent]");
//#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
//		return;
//	}

	// TCP受信応答イベントを登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_pcTcpRecvThread->m_cRecvResponseEvent.GetEventFd();
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_pcTcpRecvThread->m_cRecvResponseEvent.GetEventFd(), &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		perror("CClientResponseThread - epoll_ctl[TcpRecvResponseEvent]");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
		return;
	}

	// スレッド開始イベントを送信
	this->m_cThreadStartEvent.SetEvent();


	// ▼--------------------------------------------------------------------------▼
	// スレッド終了要求が来るまでループ
	// ※勝手にループを抜けるとスレッド終了時にタイムアウトで終了となるため、スレッド終了要求以外は勝手にループを抜けないでください
	while (bLoop) {
		memset(tEvents, 0x00, sizeof(tEvents));
		int nfds = epoll_wait(this->m_epfd, tEvents, EPOLL_MAX_EVENTS, -1);
		if (nfds == -1)
		{
			m_ErrorNo = errno;
#ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
			perror("CClientResponseThread - epoll_wait");
#endif	// #ifdef _CCLIENT_RESPONSE_THREAD_DEBUG_
			continue;
		}
		// タイムアウト
		else if (nfds == 0)
		{
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
			// クライアント切断イベント
			else if (tEvents[i].data.fd == this->m_pcTcpRecvThread->m_cClientDisconnectEvent.GetEventFd())
			{
				this->m_pcTcpRecvThread->m_cClientDisconnectEvent.ClearEvent();

				// スレッド終了フラグを立てて、ConnectMonitoringThreadにスレッド終了要求を送信して、本スレッドを終了させる
				m_bClientResponseThread_EndFlag = true;
				m_pcClientResponseThread_EndEvent->SetEvent();
				break;
			}
			//// TCP送信要求イベント
			//else if (tEvents[i].data.fd == this->m_pcTcpSendThread->m_cSendRequestEvent.GetEventFd())
			//{
			//}
			// TCP受信応答イベント
			else if (tEvents[i].data.fd == this->m_pcTcpRecvThread->m_cRecvResponseEvent.GetEventFd())
			{
				CTcpRecvThread::RECV_RESPONCE_TABLE		tRecvResponce;
				this->m_pcTcpRecvThread->GetRecvResponseData(tRecvResponce);
				printf("[%s (%d)] - %s\n", this->m_szIpAddr, this->m_Port, tRecvResponce.pRecvdData);
				free(tRecvResponce.pRecvdData);
			}
		}
	}
	// ▲--------------------------------------------------------------------------▲

	// スレッド終了イベントを送信
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// クライアント応答スレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CClientResponseThread::ThreadProcCleanup(void* pArg)
{
	CClientResponseThread* pcClientResponseThread = (CClientResponseThread*)pArg;


	// epollファイルディスクリプタ解放
	if (pcClientResponseThread->m_epfd != -1)
	{
		close(pcClientResponseThread->m_epfd);
		pcClientResponseThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// クライアント応答スレッド終了要求なのか調べる
//-----------------------------------------------------------------------------
bool CClientResponseThread::IsClientResponseThreadEnd()
{
	return m_bClientResponseThread_EndFlag;
}


