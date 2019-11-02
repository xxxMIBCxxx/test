#pragma once
//*****************************************************************************
// TCP送信スレッドクラス
// ⇒ TCP通信の送信のみを行うスレッド
//*****************************************************************************
#include "CThread.h"


class TcpSendThread : public CThread
{
public:
	TcpSendThread();
	~TcpSendThread();
};









