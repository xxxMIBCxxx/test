#pragma once
//*****************************************************************************
// �����h���[�ʐM��������
//*****************************************************************************

// �����h���[�ʐM�������ʎ��
typedef enum
{
	LANDRY_COM_RET_SECCESS = 0x00000000,										// ����I��
	LANDRY_COM_RET_RECIVE_EVENT = 0x01111111,									// �C�x���g�҂��ɂăC�x���g����M
	LANDRY_COM_RET_WAIT_TIMEOUT = 0x09999999,									// �C�x���g�҂��ɂă^�C���A�E�g������


	// ���ʂ̃G���[
	LANDRY_COM_RET_ERROR = 0xE0000000,
	LANDRY_COM_RET_ERROR_PARAM = (LANDRY_COM_RET_ERROR | 0x1),					// �p�����[�^�G���[


	// �X���b�h�֘A�̃G���[
	LANDRY_COM_RET_THREAD_ERROR = 0xE1000000,
	LANDRY_COM_RET_THREAD_ERROR_INIT = (LANDRY_COM_RET_THREAD_ERROR | 0x1),					// �X���b�h�̏��������Ɏ��s���Ă���
	LANDRY_COM_RET_THREAD_ERROR_ALREADY_STARTED = (LANDRY_COM_RET_THREAD_ERROR | 0x2),		// ���ɃX���b�h���J�n���Ă���
	LANDRY_COM_RET_THREAD_ERROR_START = (LANDRY_COM_RET_THREAD_ERROR | 0x3),				// �X���b�h�J�n�Ɏ��s���܂���
	LANDRY_COM_RET_THREAD_ERROR_START_TIMEOUT = (LANDRY_COM_RET_THREAD_ERROR | 0x4),		// �X���b�h�J�n�^�C���A�E�g
	LANDRY_COM_RET_THREAD_ERROR_NOT_ACTIVE = (LANDRY_COM_RET_THREAD_ERROR | 0x5),			// �X���b�h�����삵�Ă��Ȃ��i�܂��͏I�����Ă���j

	// �C�x���g�֘A�̃G���[
	LANDRY_COM_RET_EVENT_ERROR = 0xE2000000,
	LANDRY_COM_RET_EVENT_ERROR_FD = (LANDRY_COM_RET_EVENT_ERROR | 0x1),			// �C�x���g�t�@�C���f�B�X�N���v�^���擾�ł��Ȃ�����
	LANDRY_COM_RET_EVENT_ERROR_SET = (LANDRY_COM_RET_EVENT_ERROR | 0x2),		// �C�x���g�ݒ莸�s
	LANDRY_COM_RET_EVENT_ERROR_RESET = (LANDRY_COM_RET_EVENT_ERROR | 0x3),		// �C�x���g���Z�b�g���s
	LANDRY_COM_RET_EVENT_ERROR_WAIT = (LANDRY_COM_RET_EVENT_ERROR | 0x4),		// �C�x���g�҂����s

	// �ڑ��Ď��֘A�G���[
	LANDRY_COM_RET_ERROR_CONNECT_MONITORING_CREATE_=1,
	RESULT_ERROR_CREATE_SOCKET = 0xE1000002,								// �\�P�b�g�����Ɏ��s
	RESULT_ERROR_BIND = 0xE1000003,											// �\�P�b�g�̖��O�t���Ɏ��s
	RESULT_ERROR_LISTEN = 0xE1000004,										// �ڑ��҂��Ɏ��s
	RESULT_ERROR_SYSTEM = 0xE9999999,										// �V�X�e���G���[




	LANDRY_COM_RET_ERROR_SYSTEM = 0xE9999999,										// �V�X�e���G���[
} LANDRY_COM_RET_ENUM;





