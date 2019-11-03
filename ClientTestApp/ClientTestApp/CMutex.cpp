//*****************************************************************************
// Mutex�N���X�i�X���b�h�Ԃ̔r���̂ݎg�p�\�j
// �������J�I�v�V�����Ɂu-pthread�v��ǉ����邱��
//*****************************************************************************
#include "CMutex.h"


#define _CMUTEX_DEBUG_


//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CMutex::CMutex(int Kind)
{
	m_bInitFlag = false;
	m_ErrorNo = 0;

	int				iRet = 0;


	// �~���[�e�b�N�X�����I�u�W�F�N�g�̏�����
	iRet = pthread_mutexattr_init(&m_tMutexAttr);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CMUTEX_DEBUG_
		perror("CMutex::init - pthread_mutexattr_init");
#endif	// #ifdef _CMUTEX_DEBUG_
		return;
	}

	// �~���[�e�b�N�X�����I�u�W�F�N�g�̃^�C�v�̐ݒ�
	iRet = pthread_mutexattr_settype(&m_tMutexAttr, Kind);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CMUTEX_DEBUG_
		perror("CMutex::init - pthread_mutexattr_settype");
#endif	// #ifdef _CMUTEX_DEBUG_
		return;
	}

	// �~���[�e�b�N�X�I�u�W�F�N�g�̏�����
	iRet = pthread_mutex_init(&m_hMutex, &m_tMutexAttr);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CMUTEX_DEBUG_
		perror("CMutex::init - pthread_mutex_init");
#endif	// #ifdef _CMUTEX_DEBUG_
		return;
	}

	// ����������
	m_bInitFlag = true;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CMutex::~CMutex()
{
	// �������������Ă���ꍇ
	if (m_bInitFlag == true)
	{
		// �~���[�e�b�N�X�I�u�W�F�N�g�̉��
		pthread_mutex_destroy(&m_hMutex);
	}
}


////-----------------------------------------------------------------------------
//// �����I�u�W�F�N�g�̃^�C�v�̐ݒ�
////-----------------------------------------------------------------------------
//bool CMutex::SetType(int Kind)
//{
//	int				iRet = 0;
//
//
//	// �����I�u�W�F�N�g�̃^�C�v�̐ݒ�
//	iRet = pthread_mutexattr_settype(&m_tMutexAttr, Kind);
//	if (iRet == -1)
//	{
//		m_ErrorNo = errno;
//#ifdef _CMUTEX_DEBUG_
//		perror("CMutex::SetType - pthread_mutexattr_settype");
//#endif	// #ifdef _CMUTEX_DEBUG_
//		return false;
//	}
//
//	return true;
//}
//
//
////-----------------------------------------------------------------------------
//// �����I�u�W�F�N�g�̃^�C�v�̎擾
////-----------------------------------------------------------------------------
//bool CMutex::GetType(int* pKind)
//{
//	int				iRet = 0;
//
//
//	// �����`�F�b�N
//	if (pKind == NULL)
//	{
//		return false;
//	}
//
//	// �����I�u�W�F�N�g�̃^�C�v�̐ݒ�
//	iRet = pthread_mutexattr_gettype(&m_tMutexAttr, pKind);
//	if (iRet == -1)
//	{
//		m_ErrorNo = errno;
//#ifdef _CMUTEX_DEBUG_
//		perror("CMutex::GetType - pthread_mutexattr_gettype");
//#endif	// #ifdef _CMUTEX_DEBUG_
//		return false;
//	}
//
//	return true;
//}


//-----------------------------------------------------------------------------
// �r���J�n
//-----------------------------------------------------------------------------
bool CMutex::Lock()
{
	int					iRet = 0;


	// �r���J�n
	iRet = pthread_mutex_lock(&m_hMutex);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CMUTEX_DEBUG_
		perror("CMutex::Lock - pthread_mutex_lock");
#endif	// #ifdef _CMUTEX_DEBUG_
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// �r���I��
//-----------------------------------------------------------------------------
bool CMutex::Unlock()
{
	int					iRet = 0;


	// �r���J�n
	iRet = pthread_mutex_unlock(&m_hMutex);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CMUTEX_DEBUG_
		perror("CMutex::Unlock - pthread_mutex_unlock");
#endif	// #ifdef _CMUTEX_DEBUG_
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// �r���Ď��s
//-----------------------------------------------------------------------------
bool CMutex::TryLock()
{
	int					iRet = 0;


	// �r���J�n
	iRet = pthread_mutex_trylock(&m_hMutex);
	if (iRet == -1)
	{
		m_ErrorNo = errno;
#ifdef _CMUTEX_DEBUG_
		perror("CMutex::TryLock - pthread_mutex_trylock");
#endif	// #ifdef _CMUTEX_DEBUG_
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// �G���[�ԍ��擾
//-----------------------------------------------------------------------------
int CMutex::GetErrorNo()
{
	return m_ErrorNo;
}