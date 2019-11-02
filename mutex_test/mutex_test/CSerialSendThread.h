#pragma once
//*****************************************************************************
// シリアル通信送信スレッドクラス
//*****************************************************************************
#include "CThread.h"
#include "CEvent.h"
#include "CMutex.h"
#include "list"



class CSerialSendThread : CThread
{
public:
	// クライアント接続監視クラスの結果種別
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,										// 正常終了
		RESULT_ERROR_INIT = 0xE00000001,									// 初期処理に失敗している
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,							// 既にスレッドを開始している
		RESULT_ERROR_START = 0xE00000003,									// スレッド開始に失敗しました
		
		RESULT_ERROR_NOT_ACTIVE = 0xE1000001,								// スレッドが動作していない（または終了している）
		RESULT_ERROR_SYSTEM = 0xE9999999,									// システムエラー
	} RESULT_ENUM;

	// シリアル送信要求構造体 
	typedef struct
	{
		void*								pSenderClass;					// 送信元クラス
		ssize_t								SendDataSize;					// 送信データサイズ
		char*								pSendData;						// 送信データ（※送信する側でmallocを使用して領域を確保指定ください。解放は送信データが不要となったタイミングで本スレッド側で解放します）
	} SEND_REQUEST_TABLE;


	CEvent									m_cSendRequestEvent;			// シリアル送信要求イベント
	CMutex									m_cSendRequestListMutex;		// シリアル送信要求リスト用ミューテックス
	std::list<SEND_REQUEST_TABLE>			m_SendRequestList;				// シリアル送信要求リスト
	

private:
	bool									m_bInitFlag;					// 初期化完了フラグ
	int										m_ErrorNo;						// エラー番号
	int										m_epfd;							// epollファイルディスクリプタ（クライアント接続監視スレッドで使用）


public:
	CSerialSendThread();
	~CSerialSendThread();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	RESULT_ENUM SetSendRequestData(SEND_REQUEST_TABLE& tSendReauest);

private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void SendRequestList_Clear();

};







