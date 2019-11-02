#pragma once
//*****************************************************************************
// TCP通信受信スレッドクラス
//*****************************************************************************
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "CThread.h"
#include "CEvent.h"
#include "CMutex.h"
#include "list"


#define CTCP_RECV_THREAD_RECV_BUFF_SIZE				( 100 )
#define CTCP_RECV_THREAD_COMMAND_BUFF_SIZE			( 1000 )


class CTcpRecvThread : public CThread
{
public:
	// TCP通信受信スレッドクラスの結果種別
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,										// 正常終了
		RESULT_ERROR_INIT = 0xE00000001,									// 初期処理に失敗している
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,							// 既にスレッドを開始している
		RESULT_ERROR_START = 0xE00000003,									// スレッド開始に失敗しました

		RESULT_ERROR_NOT_ACTIVE = 0xE1000001,								// スレッドが動作していない（または終了している）
		RESULT_ERROR_PARAM = 0xE1000002,									// パラメータエラー
		RESULT_ERROR_RECV = 0xE1000003,										// TCP受信エラー
		RESULT_ERROR_COMMAND_BUFF_OVER = 0xE1000004,						// コマンドバッファオーバー
		RESULT_ERROR_SYSTEM = 0xE9999999,									// システムエラー
	} RESULT_ENUM;


	// クライアント情報構造体
	typedef struct
	{
		int									Socket;							// ソケット
		struct sockaddr_in					tAddr;							// インターネットソケットアドレス構造体

	} CLIENT_INFO_TABLE;

	// TCP受信応答構造体 
	typedef struct
	{
		void*								pReceverClass;					// 受信先クラス
		ssize_t								RecvDataSize;					// 受信データサイズ
		char*								pRecvdData;						// 受信データ（※受信先にてデータが不要となったら、freeを使用して領域を解放してください）
	} RECV_RESPONCE_TABLE;

	// 解析種別
	typedef enum
	{
		ANALYZE_KIND_STX = 0,												// STX待ち
		ANALYZE_KIND_ETX = 1,												// ETX待ち
	} ANALYZE_KIND_ENUM;




	CEvent									m_cRecvResponseEvent;			// TCP受信応答イベント
	CMutex									m_cRecvResponseListMutex;		// TCP受信応答リスト用ミューテックス
	std::list<RECV_RESPONCE_TABLE>			m_RecvResponseList;				// TCP受信応答リスト

	
	ANALYZE_KIND_ENUM						m_eAnalyzeKind;												// 解析種別
	char									m_szRecvBuff[CTCP_RECV_THREAD_RECV_BUFF_SIZE + 1];			// 受信バッファ
	char									m_szCommandBuff[CTCP_RECV_THREAD_COMMAND_BUFF_SIZE + 1];	// 受信コマンドバッファ
	ssize_t									m_CommandPos;												// 受信コマンド格納位置

private:
	bool									m_bInitFlag;					// 初期化完了フラグ
	int										m_ErrorNo;						// エラー番号
	int										m_epfd;							// epollファイルディスクリプタ（クライアント接続監視スレッドで使用）
	CLIENT_INFO_TABLE						m_tClientInfo;					// クライアント情報

public:
	CTcpRecvThread(CLIENT_INFO_TABLE &tClientInfo);
	~CTcpRecvThread();
	int GetErrorNo();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();
	RESULT_ENUM GetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce);
	RESULT_ENUM SetRecvResponseData(RECV_RESPONCE_TABLE& tRecvResponce);


private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	void RecvResponseList_Clear();
	RESULT_ENUM TcpRecvProc();
	RESULT_ENUM TcpRecvDataAnalyze(char* pRecvData, ssize_t RecvDataNum);
};


