#pragma once
//*****************************************************************************
// Event�N���X
//*****************************************************************************
#include <cstdio>

class CEvent
{
public:
	typedef enum
	{
		RESULT_SUCCESS = 0x00000000,					// ����
		RESULT_RECIVE_EVENT = 0x01111111,				// �C�x���g�҂��ɂăC�x���g����M
		RESULT_WAIT_TIMEOUT = 0x09999999,				// �C�x���g�҂��ɂă^�C���A�E�g������

		RESULT_ERROR_EVENT_FD = 0xE0000001,				// �C�x���g�t�@�C���f�B�X�N���v�^���擾�ł��Ȃ�����
		RESULT_ERROR_EVENT_SET = 0xE0000002,			// �C�x���g�ݒ莸�s
		RESULT_ERROR_EVENT_RESET = 0xE0000003,			// �C�x���g���Z�b�g���s
		RESULT_ERROR_EVENT_WAIT = 0xE0000004,			// �C�x���g�҂����s
		RESULT_ERROR_SYSTEM = 0xE9999999,				// �V�X�e���ُ�
	} RESULT_ENUM;


private:
	int								m_efd;				// �C�x���g�t�@�C���f�B�X�N���v�^
	int								m_ErrorNo;			// �G���[�ԍ�

public:
	CEvent();
	~CEvent();
	RESULT_ENUM Init();
	int GetEventFd();
	int GetErrorNo();
	RESULT_ENUM SetEvent();
	RESULT_ENUM ResetEvent();
	RESULT_ENUM Wait(unsigned int Timeout = 0);
};

