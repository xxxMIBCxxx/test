#pragma once
//*****************************************************************************
// Mutex�N���X�i�X���b�h�Ԃ̔r���̂ݎg�p�\�j
// �������J�I�v�V�����Ɂu-pthread�v��ǉ����邱��
//*****************************************************************************
#include <cstdio>
#include <pthread.h>
#include <errno.h> 


class CMutex
{
private:
	bool								m_bInitFlag;			// �������t���O
	int									m_ErrorNo;				// �G���[�ԍ�

	pthread_mutexattr_t					m_tMutexAttr;			// �~���[�e�b�N�X����
	pthread_mutex_t						m_hMutex;				// �~���[�e�b�N�X�n���h��


public:
	CMutex(int Kind = PTHREAD_PROCESS_PRIVATE);
	~CMutex();
//	bool SetType(int Kind = PTHREAD_PROCESS_PRIVATE);
//	bool GetType(int* pKind);
	bool Lock();
	bool Unlock();
	bool TryLock();
	int GetErrorNo();
};