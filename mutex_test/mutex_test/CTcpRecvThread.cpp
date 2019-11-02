//*****************************************************************************
// TCP通信受信スレッドクラス
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include "CTcpRecvThread.h"


#define _CTCP_RECV_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント
#define STX											( 0x02 )					// STX
#define ETX											( 0x03 )					// ETX

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CTcpRecvThread::CTcpRecvThread(CLIENT_INFO_TABLE& tClientInfo)
{
	bool						bRet = false;
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;
	m_tClientInfo = tClientInfo;
	m_eAnalyzeKind = ANALYZE_KIND_STX;
	m_CommandPos = 0;
	memset(m_szRecvBuff, 0x00, sizeof(m_szRecvBuff));
	memset(m_szCommandBuff, 0x00, sizeof(m_szCommandBuff));

	// TCP受信応答イベントの初期化
//	eEventRet = m_cRecvResponseEvent.Init(EFD_SEMAPHORE);
	eEventRet = m_cRecvResponseEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// TCP受信応答リストのクリア
	m_RecvResponseList.clear();


	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CTcpRecvThread::~CTcpRecvThread()
{
	// TCP通信受信スレッド停止し忘れ考慮
	this->Stop();

	// 再度、TCP受信応答リストをクリアする
	RecvResponseList_Clear();
}


//-----------------------------------------------------------------------------
// エラー番号を取得
//-----------------------------------------------------------------------------
int CTcpRecvThread::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// TCP通信受信スレッド開始
//-----------------------------------------------------------------------------
CTcpRecvThread::RESULT_ENUM CTcpRecvThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_RECV_THREAD_DEBUG_
		printf("CTcpRecvThread::Start - Not Init Proc.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが動作している場合
	bRet = this->IsActive();
	if (bRet == true)
	{
#ifdef _CTCP_RECV_THREAD_DEBUG_
		printf("CTcpRecvThread::Start - Thread Active.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// TCP通信受信スレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CTcpRecvThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP通信受信スレッド停止
//-----------------------------------------------------------------------------
CTcpRecvThread::RESULT_ENUM CTcpRecvThread::Stop()
{
	bool						bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_RECV_THREAD_DEBUG_
		printf("CTcpRecvThread::Stop - Not Init Proc.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_SUCCESS;
	}

	// TCP通信受信スレッド停止
	CThread::Stop();

	// TCP受信応答リストをクリアする
	RecvResponseList_Clear();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP通信受信スレッド
//-----------------------------------------------------------------------------
void CTcpRecvThread::ThreadProc()
{
	int							iRet = 0;
	struct epoll_event			tEvent;
	struct epoll_event			tEvents[EPOLL_MAX_EVENTS];
	bool						bLoop = true;


	// スレッドが終了する際に呼ばれる関数を登録
	pthread_cleanup_push(ThreadProcCleanup, this);

	// epollファイルディスクリプタ生成
	m_epfd = epoll_create(EPOLL_MAX_EVENTS);
	if (m_epfd == -1)
	{
		m_ErrorNo = errno;
#ifdef _CTCP_RECV_THREAD_DEBUG_
		perror("CTcpRecvThread::ThreadProc - epoll_create");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
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
#ifdef _CTCP_RECV_THREAD_DEBUG_
		perror("CTcpRecvThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
		return;
	}

	// TCP受信用のソケットを登録
	memset(&tEvent, 0x00, sizeof(tEvent));
	tEvent.events = EPOLLIN;
	tEvent.data.fd = this->m_tClientInfo.Socket;
	iRet = epoll_ctl(m_epfd, EPOLL_CTL_ADD, this->m_tClientInfo.Socket, &tEvent);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CTCP_RECV_THREAD_DEBUG_
		perror("CTcpRecvThread::ThreadProc - epoll_ctl[RecvResponseEvent]");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
		return;
	}

	// スレッド開始イベントを送信
	this->m_cThreadStartEvent.SetEvent();

	// ▼--------------------------------------------------------------------------▼
	// スレッド終了要求が来るまでループ
	// ※勝手にループを抜けるとスレッド終了時にタイムアウトで終了となるため、スレッド終了要求以外は勝手にループを抜けないでください
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
			// スレッド終了要求イベント受信
			if (tEvents[i].data.fd == this->GetThreadEndReqEventFd())
			{
				bLoop = false;
				break;
			}
			// TCP受信用のソケット
			else if (this->m_tClientInfo.Socket)
			{
				// TCP受信処理
				TcpRecvProc();
			}
		}
	}

	// スレッド終了イベントを送信
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// TCP通信受信スレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CTcpRecvThread::ThreadProcCleanup(void* pArg)
{
	// パラメータチェック
	if (pArg == NULL)
	{
		return;
	}
	CTcpRecvThread* pcTcpRecvThread = (CTcpRecvThread*)pArg;


	// epollファイルディスクリプタ解放
	if (pcTcpRecvThread->m_epfd != -1)
	{
		close(pcTcpRecvThread->m_epfd);
		pcTcpRecvThread->m_epfd = -1;
	}
}


//-----------------------------------------------------------------------------
// TCP受信応答リストをクリアする
//-----------------------------------------------------------------------------
void CTcpRecvThread::RecvResponseList_Clear()
{
	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cRecvResponseListMutex.Lock();

	std::list<RECV_RESPONCE_TABLE>::iterator		it = m_RecvResponseList.begin();
	while (it != m_RecvResponseList.end())
	{
		RECV_RESPONCE_TABLE			tRecvResponse = *it;

		// バッファが確保されている場合
		if (tRecvResponse.pRecvdData != NULL)
		{
			// バッファ領域を解放
			free(tRecvResponse.pRecvdData);
		}
		it++;
	}

	// TCP受信応答リストをクリア
	m_RecvResponseList.clear();

	m_cRecvResponseListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲
}


//-----------------------------------------------------------------------------
// TCP受信データ取得
//-----------------------------------------------------------------------------
CTcpRecvThread::RESULT_ENUM CTcpRecvThread::GetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce)
{
	bool					bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_RECV_THREAD_DEBUG_
		printf("CTcpRecvThread::GetRecvResponseData - Not Init Proc.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_ERROR_NOT_ACTIVE;
	}

	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cRecvResponseListMutex.Lock();

	// TCP受信応答リストに登録データある場合
	if (m_RecvResponseList.empty() != true)
	{
		// TCP受信応答リストの先頭データを取り出す（※リストの先頭データは削除）
		std::list<RECV_RESPONCE_TABLE>::iterator		it = m_RecvResponseList.begin();
		tRecvResponce = *it;
		m_RecvResponseList.pop_front();
	}

	m_cRecvResponseListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	// TCP受信応答データを取得したので、TCP受信応答イベントをクリアする
	m_cRecvResponseEvent.ClearEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP受信データ設定
//-----------------------------------------------------------------------------
CTcpRecvThread::RESULT_ENUM CTcpRecvThread::SetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce)
{
	bool					bRet = false;


	// 引数チェック
	if (tRecvResponce.pRecvdData == NULL)
	{
		return RESULT_ERROR_PARAM;
	}

	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CTCP_RECV_THREAD_DEBUG_
		printf("CTcpRecvThread::SetRecvResponseData - Not Init Proc.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_ERROR_NOT_ACTIVE;
	}

	// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼
	m_cRecvResponseListMutex.Lock();

	// TCP受信応答リストに登録
	m_RecvResponseList.push_back(tRecvResponce);

	m_cRecvResponseListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	// TCP受信応答イベントを送信する
	m_cRecvResponseEvent.SetEvent();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP受信処理
//-----------------------------------------------------------------------------
CTcpRecvThread::RESULT_ENUM CTcpRecvThread::TcpRecvProc()
{
	ssize_t					read_count = 0;
	// TCP受信処理
	memset(m_szRecvBuff, 0x00, sizeof(m_szRecvBuff));
	read_count = read(m_tClientInfo.Socket, m_szRecvBuff, CTCP_RECV_THREAD_RECV_BUFF_SIZE);
	if (read_count < 0)
	{
		m_ErrorNo = errno;
#ifdef _CTCP_RECV_THREAD_DEBUG_
		perror("CTcpRecvThread::TcpRecvProc - read");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
		return RESULT_ERROR_RECV;
	}
	else if (read_count == 0)
	{
		printf("read_count = 0\n");

	}
	else
	{
		// TCP受信データ解析
		TcpRecvDataAnalyze(m_szRecvBuff, read_count);
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// TCP受信データ解析
//-----------------------------------------------------------------------------
CTcpRecvThread::RESULT_ENUM CTcpRecvThread::TcpRecvDataAnalyze(char* pRecvData, ssize_t RecvDataNum)
{
	RESULT_ENUM					eRet = RESULT_SUCCESS;

	// パラメータチェック
	if (pRecvData == NULL)
	{
		return RESULT_ERROR_PARAM;
	}

	// 受信したデータを1Byteずつ調べる
	for (ssize_t i = 0; i < RecvDataNum; i++)
	{
		char		ch = pRecvData[i];


		// 解析種別によって処理を変更
		if (m_eAnalyzeKind == ANALYZE_KIND_STX)
		{
			// STX
			if (ch == STX)
			{
				m_CommandPos = 0;
				memset(m_szCommandBuff, 0x00, sizeof(m_szCommandBuff));
				m_szCommandBuff[m_CommandPos++] = ch;
				m_eAnalyzeKind = ANALYZE_KIND_ETX;
			}
			else
			{
				// 受信データを破棄
			}
		}
		else
		{
			// STX
			if (ch == STX)
			{
				m_CommandPos = 0;
				memset(m_szCommandBuff, 0x00, sizeof(m_szCommandBuff));
				m_szCommandBuff[m_CommandPos++] = ch;
				m_eAnalyzeKind = ANALYZE_KIND_ETX;
			}
			else if (ch == ETX)
			{
				// 解析完了
				m_szCommandBuff[m_CommandPos++] = ch;
				m_eAnalyzeKind = ANALYZE_KIND_STX;

				// TCP受信応答（※受信データをClientResponseThread側へ渡す）
				RECV_RESPONCE_TABLE		tRecvResponce;
				memset(&tRecvResponce, 0x00, sizeof(tRecvResponce));
				tRecvResponce.pReceverClass = this;
				tRecvResponce.RecvDataSize = strlen(m_szCommandBuff) + 1;			// Debug用
//				tRecvResponce.RecvDataSize = strlen(m_szCommandBuff);
				tRecvResponce.pRecvdData = (char*)malloc(tRecvResponce.RecvDataSize);
				if (tRecvResponce.pRecvdData == NULL)
				{
#ifdef _CTCP_RECV_THREAD_DEBUG_
					printf("CTcpRecvThread::TcpRecvDataAnalyze - malloc error.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
					return RESULT_ERROR_SYSTEM;
				}
				memcpy(tRecvResponce.pRecvdData, m_szCommandBuff, tRecvResponce.RecvDataSize);
				eRet = this->SetRecvResponseData(tRecvResponce);
				if (eRet != RESULT_SUCCESS)
				{
#ifdef _CTCP_RECV_THREAD_DEBUG_
					printf("CTcpRecvThread::TcpRecvDataAnalyze - SetRecvResponseData error.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
					return eRet;
				}
			}
			else
			{
				// 受信データを格納
				m_szCommandBuff[m_CommandPos++] = ch;
				if (m_CommandPos >= CTCP_RECV_THREAD_COMMAND_BUFF_SIZE)
				{
#ifdef _CTCP_RECV_THREAD_DEBUG_
					printf("CTcpRecvThread::TcpRecvDataAnalyze - Command Buffer Over.\n");
#endif	// #ifdef _CTCP_RECV_THREAD_DEBUG_
					return RESULT_ERROR_COMMAND_BUFF_OVER;
				}
			}
		}
	}

	return RESULT_SUCCESS;
}
















