#pragma once
//*****************************************************************************
// TCP通信受信スレッドクラス
//*****************************************************************************
#include "CThread.h"
#include "CEvent.h"
#include "CMutex.h"
#include "list"


class CTcpRecvThread : public CThread
{
	// シリアル通信受信スレッドクラスの結果種別
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,										// 正常終了
		RESULT_ERROR_INIT = 0xE00000001,									// 初期処理に失敗している
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,							// 既にスレッドを開始している
		RESULT_ERROR_START = 0xE00000003,									// スレッド開始に失敗しました

		RESULT_ERROR_NOT_ACTIVE = 0xE1000001,								// スレッドが動作していない（または終了している）
		RESULT_ERROR_PARAM = 0xE1000002,									// パラメータエラー
		RESULT_ERROR_SYSTEM = 0xE9999999,									// システムエラー
	} RESULT_ENUM;

	// シリアル受信応答構造体 
	typedef struct
	{
		void*								pReceverClass;					// 受信先クラス
		ssize_t								RecvDataSize;					// 受信データサイズ
		char*								pRecvdData;						// 受信データ（※受信先にてデータが不要となったら、freeを使用して領域を解放してください）
	} RECV_RESPONCE_TABLE;


	CEvent									m_cRecvResponseEvent;			// TCP受信応答イベント
	CMutex									m_cRecvResponseListMutex;		// TCP受信応答リスト用ミューテックス
	std::list<RECV_RESPONCE_TABLE>			m_RecvResponseList;				// TCP受信応答リスト


private:
	bool									m_bInitFlag;					// 初期化完了フラグ
	int										m_ErrorNo;						// エラー番号
	int										m_epfd;							// epollファイルディスクリプタ（クライアント接続監視スレッドで使用）

public:
	CTcpRecvThread();
	~CTcpRecvThread();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	RESULT_ENUM GetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce);
	RESULT_ENUM SetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce);

private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void RecvResponseList_Clear();
};


