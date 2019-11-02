//*****************************************************************************
// シリアル通信受信スレッドクラス
//*****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include "CSerialRecvThread.h"


#define _CSERIAL_RECV_THREAD_DEBUG_
#define EPOLL_MAX_EVENTS							( 10 )						// epoll最大イベント


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CSerialRecvThread::CSerialRecvThread()
{
	bool						bRet = false;
	CEvent::RESULT_ENUM			eEventRet = CEvent::RESULT_SUCCESS;


	m_bInitFlag = false;
	m_ErrorNo = 0;
	m_epfd = -1;


	// シリアル受信応答イベントの初期化
	eEventRet = m_cRecvResponseEvent.Init();
	if (eEventRet != CEvent::RESULT_SUCCESS)
	{
		return;
	}

	// シリアル受信応答リストのクリア
	m_RecvResponseList.clear();


	// 初期化完了
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CSerialRecvThread::~CSerialRecvThread()
{
	// シリアル通信受信スレッド停止し忘れ考慮
	this->Stop();

	// 再度、シリアル受信応答リストをクリアする
	RecvResponseList_Clear();
}


//-----------------------------------------------------------------------------
// シリアル通信受信スレッド開始
//-----------------------------------------------------------------------------
CSerialRecvThread::RESULT_ENUM CSerialRecvThread::Start()
{
	bool						bRet = false;
	RESULT_ENUM					eRet = RESULT_SUCCESS;
	CThread::RESULT_ENUM		eThreadRet = CThread::RESULT_SUCCESS;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::Start - Not Init Proc.\n");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが動作している場合
	bRet = this->IsActive();
	if (bRet == true)
	{
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::Start - Thread Active.\n");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return RESULT_ERROR_ALREADY_STARTED;
	}

	// シリアル通信受信スレッド開始
	eThreadRet = CThread::Start();
	if (eThreadRet != CThread::RESULT_SUCCESS)
	{
		m_ErrorNo = CThread::GetErrorNo();
		return (CSerialRecvThread::RESULT_ENUM)eThreadRet;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// シリアル通信受信スレッド停止
//-----------------------------------------------------------------------------
CSerialRecvThread::RESULT_ENUM CSerialRecvThread::Stop()
{
	bool						bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::Stop - Not Init Proc.\n");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return RESULT_ERROR_INIT;
	}

	// 既にスレッドが停止している場合
	bRet = this->IsActive();
	if (bRet == false)
	{
		return RESULT_SUCCESS;
	}

	// シリアル通信受信スレッド停止
	CThread::Stop();

	// シリアル受信応答リストをクリアする
	RecvResponseList_Clear();

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// シリアル通信受信スレッド
//-----------------------------------------------------------------------------
void CSerialRecvThread::ThreadProc()
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
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		perror("CSerialRecvThread::ThreadProc - epoll_create");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
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
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		perror("CSerialRecvThread::ThreadProc - epoll_ctl[ThreadEndReqEvent]");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
		return;
	}

	// シリアル受信応答イベントを登録
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
			// スレッド終了要求イベント受信
			if (tEvents[i].data.fd == this->GetThreadEndReqEventFd())
			{
				bLoop = false;
				break;
			}
			// シリアル受信応答イベント受信
			else if (tEvents[i].data.fd == this->m_cRecvResponseEvent.GetEventFd())
			{
				// ★シリアル受信処理
			}
		}
	}

	// スレッド終了イベントを送信
	this->m_cThreadEndEvent.SetEvent();

	pthread_cleanup_pop(1);
}


//-----------------------------------------------------------------------------
// シリアル通信受信スレッド終了時に呼ばれる処理
//-----------------------------------------------------------------------------
void CSerialRecvThread::ThreadProcCleanup(void* pArg)
{
	// パラメータチェック
	if (pArg == NULL)
	{
		return;
	}
	CSerialRecvThread* pcSerialRecvThread = (CSerialRecvThread*)pArg;


	// epollファイルディスクリプタ解放
	if (pcSerialRecvThread->m_epfd != -1)
	{
		close(pcSerialRecvThread->m_epfd);
		pcSerialRecvThread->m_epfd = -1;
	}
}



//-----------------------------------------------------------------------------
// シリアル受信応答リストをクリアする
//-----------------------------------------------------------------------------
void CSerialRecvThread::RecvResponseList_Clear()
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

	// シリアル受信応答リストをクリア
	m_RecvResponseList.clear();

	m_cRecvResponseListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲
}


//-----------------------------------------------------------------------------
// シリアル受信データ取得
//-----------------------------------------------------------------------------
CSerialRecvThread::RESULT_ENUM CSerialRecvThread::GetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce)
{
	bool					bRet = false;


	// 初期化処理が完了していない場合
	if (m_bInitFlag == false)
	{
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::GetRecvResponseData - Not Init Proc.\n");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
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

	// シリアル受信応答リストに登録データある場合
	if (m_RecvResponseList.empty() != true)
	{
		// シリアル受信応答リストの先頭データを取り出す（※リストの先頭データは削除）
		std::list<RECV_RESPONCE_TABLE>::iterator		it = m_RecvResponseList.begin();
		tRecvResponce = *it;
		m_RecvResponseList.pop_front();
	}

	m_cRecvResponseListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// シリアル受信データ設定
//-----------------------------------------------------------------------------
CSerialRecvThread::RESULT_ENUM CSerialRecvThread::SetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce)
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
#ifdef _CSERIAL_RECV_THREAD_DEBUG_
		printf("CSerialRecvThread::SetRecvResponseData - Not Init Proc.\n");
#endif	// #ifdef _CSERIAL_RECV_THREAD_DEBUG_
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

	// シリアル受信応答リストに登録
	m_RecvResponseList.push_back(tRecvResponce);

	m_cRecvResponseListMutex.Unlock();
	// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲

	// シリアル受信応答イベントを送信する
	m_cRecvResponseEvent.SetEvent();

	return RESULT_SUCCESS;
}