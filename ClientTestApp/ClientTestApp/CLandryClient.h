#pragma once
//*****************************************************************************
// LandryClient
//*****************************************************************************
#include "CThread.h"
#include "CEvent.h"
#include "CClientConnectMonitoringThread.h"


class CLandryClient : public CThread
{
	// LandryClient�N���X�̌��ʎ��
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,											// ����I��
		RESULT_ERROR_INIT = 0xE00000001,										// ���������Ɏ��s���Ă���
		RESULT_ERROR_ALREADY_STARTED = 0xE00000002,								// ���ɃX���b�h���J�n���Ă���
		RESULT_ERROR_START = 0xE00000003,										// �X���b�h�J�n�Ɏ��s���܂���

		RESULT_ERROR_PARAM = 0xE1000001,										// �p�����[�^�G���[
		RESULT_ERROR_CREATE_SOCKET = 0xE1000002,								// �\�P�b�g�����Ɏ��s
		RESULT_ERROR_CONNECT = 0xE1000003,										// �ڑ��Ɏ��s
		RESULT_ERROR_LISTEN = 0xE1000004,										// �ڑ��҂��Ɏ��s

		RESULT_ERROR_SYSTEM = 0xE9999999,										// �V�X�e���G���[
	} RESULT_ENUM;


	CEvent									m_cServerDisconnectEvent;			// �T�[�o�[�ؒf�C�x���g
	CClientConnectMonitoringThread*			m_pcClinentConnectMonitoringThread; // �ڑ��Ď��X���b�h�i�N���C�A���g�Łj


private:
	bool									m_bInitFlag;						// �����������t���O
	int										m_ErrorNo;							// �G���[�ԍ�
	int										m_epfd;								// epoll�t�@�C���f�B�X�N���v�^�i�N���C�A���g�����X���b�h�Ŏg�p�j

public:
	CLandryClient();
	~CLandryClient();
	RESULT_ENUM Start();
	RESULT_ENUM Stop();

private:
	void ThreadProc();
	static void ThreadProcCleanup(void* pArg);
	RESULT_ENUM CreateClientMonitoringThread();
	void DeleteClientMonitoringThread();
};



