#pragma once
//*****************************************************************************
// ランドリー通信処理結果
//*****************************************************************************

// ランドリー通信処理結果種別
typedef enum
{
	LANDRY_COM_RET_SECCESS = 0x00000000,										// 正常終了
	LANDRY_COM_RET_RECIVE_EVENT = 0x01111111,									// イベント待ちにてイベントを受信
	LANDRY_COM_RET_WAIT_TIMEOUT = 0x09999999,									// イベント待ちにてタイムアウトが発生


	// 共通のエラー
	LANDRY_COM_RET_ERROR = 0xE0000000,
	LANDRY_COM_RET_ERROR_PARAM = (LANDRY_COM_RET_ERROR | 0x1),					// パラメータエラー


	// スレッド関連のエラー
	LANDRY_COM_RET_THREAD_ERROR = 0xE1000000,
	LANDRY_COM_RET_THREAD_ERROR_INIT = (LANDRY_COM_RET_THREAD_ERROR | 0x1),					// スレッドの初期処理に失敗している
	LANDRY_COM_RET_THREAD_ERROR_ALREADY_STARTED = (LANDRY_COM_RET_THREAD_ERROR | 0x2),		// 既にスレッドを開始している
	LANDRY_COM_RET_THREAD_ERROR_START = (LANDRY_COM_RET_THREAD_ERROR | 0x3),				// スレッド開始に失敗しました
	LANDRY_COM_RET_THREAD_ERROR_START_TIMEOUT = (LANDRY_COM_RET_THREAD_ERROR | 0x4),		// スレッド開始タイムアウト
	LANDRY_COM_RET_THREAD_ERROR_NOT_ACTIVE = (LANDRY_COM_RET_THREAD_ERROR | 0x5),			// スレッドが動作していない（または終了している）

	// イベント関連のエラー
	LANDRY_COM_RET_EVENT_ERROR = 0xE2000000,
	LANDRY_COM_RET_EVENT_ERROR_FD = (LANDRY_COM_RET_EVENT_ERROR | 0x1),			// イベントファイルディスクリプタが取得できなかった
	LANDRY_COM_RET_EVENT_ERROR_SET = (LANDRY_COM_RET_EVENT_ERROR | 0x2),		// イベント設定失敗
	LANDRY_COM_RET_EVENT_ERROR_RESET = (LANDRY_COM_RET_EVENT_ERROR | 0x3),		// イベントリセット失敗
	LANDRY_COM_RET_EVENT_ERROR_WAIT = (LANDRY_COM_RET_EVENT_ERROR | 0x4),		// イベント待ち失敗

	// 接続監視関連エラー
	LANDRY_COM_RET_ERROR_CONNECT_MONITORING_CREATE_=1,
	RESULT_ERROR_CREATE_SOCKET = 0xE1000002,								// ソケット生成に失敗
	RESULT_ERROR_BIND = 0xE1000003,											// ソケットの名前付けに失敗
	RESULT_ERROR_LISTEN = 0xE1000004,										// 接続待ちに失敗
	RESULT_ERROR_SYSTEM = 0xE9999999,										// システムエラー




	LANDRY_COM_RET_ERROR_SYSTEM = 0xE9999999,										// システムエラー
} LANDRY_COM_RET_ENUM;





