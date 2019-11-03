//*****************************************************************************
// 接続監視スレッドクラス（クライアント版）
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
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント

//-----------------------------------------------------------------------------
// コンストラクタ
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

	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CClientConnectMonitoringThread::~CClientConnectMonitoringThread()
{
	// 接続監視スレッド停止漏れ考慮
	this->Stop();
}


//-----------------------------------------------------------------------------
// 接続監視スレッド開始
//-----------------------------------------------------------------------------
CClientConnectMonitoringThread::RESULT_ENUM CClientConnectMonitoringThread::Start()
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

	// サーバー接続処理
	eRet = ServerConnect(m_tServerInfo);
	if (eRet != RESULT_SUCCESS)
	{
		return eRet;
	}

	// 接続監視スレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();

		// サーバー切断処理
		ServerDisconnect(m_tServerInfo);

		return (CClientConnectMonitoringThread::RESULT_ENUM)eThreadRet;
	}

	// TCP受信・送信スレッド生成 & 開始
	eRet = CreateTcpThread(m_tServerInfo);
	if (eRet != RESULT_SUCCESS)
	{
		// 接続監視スレッド停止
		CThread::Stop();

		// サーバー切断処理
		ServerDisconnect(m_tServerInfo);

		return eRet;
	}


	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// 接続監視スレッド停止
//-----------------------------------------------------------------------------
CClientConnectMonitoringThread::RESULT_ENUM CClientConnectMonitoringThread::Stop()
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

	// TCP受信・送信スレッド解放 & 停止
	DeleteTcpThread();

	// 接続監視スレッド停止
	CThread::Stop();

	// サーバー切断処理
	ServerDisconnect(m_tServerInfo);

	return RESULT_SUCCESS;
}



//-----------------------------------------------------------------------------
// 接続監視スレッド
//-----------------------------------------------------------------------------
void CClientConnectMonitoringThread::ThreadProc()
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
#ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CClientConnectMonitoringThread::ThreadProc - epoll_create");
#endif	// #ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
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
#ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CClientConnectMonitoringThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
		return;
	}

	// サーバー情報を取得
	sprintf(m_szIpAddr, "%s", inet_ntoa(m_tServerInfo.tAddr.sin_addr));			// IPアドレス取得
	m_Port = ntohs(m_tServerInfo.tAddr.sin_port);								// ポート番号取得

	printf("[%s (%d)] - Server Connect!\n", m_szIpAddr, m_Port);

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
#ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
			perror("CClientConnectMonitoringThread::ThreadProc - epoll_wait");
#endif	// #ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
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
		}
	}
	// ▲--------------------------------------------------------------------------▲

	// スレッド終了イベントを送信
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// 接続監視スレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CClientConnectMonitoringThread::ThreadProcCleanup(void* pArg)
{
	CClientConnectMonitoringThread* pcClientConnectMonitoringThread = (CClientConnectMonitoringThread*)pArg;


	// epollファイルディスクリプタ解放
	if (pcClientConnectMonitoringThread->m_epfd != -1)
	{
		close(pcClientConnectMonitoringThread->m_epfd);
		pcClientConnectMonitoringThread->m_epfd = -1;
	}
}









//-----------------------------------------------------------------------------
// サーバー接続処理
//-----------------------------------------------------------------------------
CClientConnectMonitoringThread::RESULT_ENUM CClientConnectMonitoringThread::ServerConnect(SERVER_INFO_TABLE& tServerInfo)
{
	int					iRet = 0;


	// ソケットを生成
	tServerInfo.Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (tServerInfo.Socket == -1)
	{
		m_ErrorNo = errno;
#ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CClientConnectMonitoringThread::ServerConnect - socket");
#endif	// #ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
		return RESULT_ERROR_CREATE_SOCKET;
	}

	// サーバー接続先を設定
	tServerInfo.tAddr.sin_family = AF_INET;
	tServerInfo.tAddr.sin_port = htons(12345);							// ← ポート番号
	tServerInfo.tAddr.sin_addr.s_addr = inet_addr("192.168.10.9");		// ← IPアドレス
	if (tServerInfo.tAddr.sin_addr.s_addr == 0xFFFFFFFF)
	{
		// ホスト名からIPアドレスを取得
		struct hostent* host;
		host = gethostbyname("localhost");
		tServerInfo.tAddr.sin_addr.s_addr = *(unsigned int*)host->h_addr_list[0];
	}

	// サーバーに接続する
	iRet = connect(tServerInfo.Socket, (struct sockaddr*) & tServerInfo.tAddr, sizeof(tServerInfo.tAddr));
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_
		perror("CClientConnectMonitoringThread::ServerConnect - connect");
#endif	// #ifdef _CCLIENT_CONNECT_MONITORING_THREAD_DEBUG_

		// サーバー切断処理
		ServerDisconnect(tServerInfo);

		return RESULT_ERROR_CONNECT;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// サーバー切断処理
//-----------------------------------------------------------------------------
void CClientConnectMonitoringThread::ServerDisconnect(SERVER_INFO_TABLE& tServerInfo)
{
	// ソケットが生成されている場合
	if (tServerInfo.Socket != -1)
	{
		close(tServerInfo.Socket);
	}

	// サーバー情報初期化
	memset(&tServerInfo, 0x00, sizeof(tServerInfo));
	tServerInfo.Socket = -1;
}


//-----------------------------------------------------------------------------
// TCP受信・送信スレッド生成 & 開始
//-----------------------------------------------------------------------------
CClientConnectMonitoringThread::RESULT_ENUM CClientConnectMonitoringThread::CreateTcpThread(SERVER_INFO_TABLE& tServerInfo)
{
	// TCP受信スレッド生成
	CTcpRecvThread::SERVER_INFO_TABLE		tTcpRecvServerInfo;
	tTcpRecvServerInfo.Socket = m_tServerInfo.Socket;
	memcpy(&tTcpRecvServerInfo.tAddr, &m_tServerInfo.tAddr, sizeof(tTcpRecvServerInfo.tAddr));
	m_pcTcpRecvThread = (CTcpRecvThread*)new CTcpRecvThread(tTcpRecvServerInfo, m_pcServerDisconnectEvent);
	if (m_pcTcpRecvThread == NULL)
	{
		return RESULT_ERROR_SYSTEM;
	}

	// TCP受信スレッド開始
	CTcpRecvThread::RESULT_ENUM eTcpRecvThreadRet = m_pcTcpRecvThread->Start();
	if (eTcpRecvThreadRet != CTcpRecvThread::RESULT_SUCCESS)
	{
		// TCP受信・送信スレッド解放 & 停止
		DeleteTcpThread();

		return (CClientConnectMonitoringThread::RESULT_ENUM)eTcpRecvThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP受信・送信スレッド解放 & 停止
//-----------------------------------------------------------------------------
void CClientConnectMonitoringThread::DeleteTcpThread()
{
	// TCP受信スレッド解放
	if (m_pcTcpRecvThread != NULL)
	{
		delete m_pcTcpRecvThread;
		m_pcTcpRecvThread = NULL;
	}
}
