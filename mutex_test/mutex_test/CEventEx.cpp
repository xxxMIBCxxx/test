//*****************************************************************************
// EventEx�N���X
//*****************************************************************************
#include "CEventEx.h"
#include <sys/eventfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


#define _CEVENT_EX_DEBUG_
#define MONITORING_COUNTER_NUM				( 20 )

//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CEventEx::CEventEx()
{
	m_ErrorNo = 0;
	m_efd = -1;
	m_Counter = 0;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CEventEx::~CEventEx()
{
	// �C�x���g�t�@�C���f�B�X�N���v�^�����
	if (m_efd == -1)
	{
		close(m_efd);
		m_efd = -1;
	}
}


//-----------------------------------------------------------------------------
// ��������
//-----------------------------------------------------------------------------
CEventEx::RESULT_ENUM CEventEx::Init()
{
	// �C�x���g�t�@�C���f�B�X�N���v�^�𐶐�
	m_efd = eventfd(EFD_SEMAPHORE, 0);
	if (m_efd == -1)
	{
		m_ErrorNo = errno;
#ifdef _CEVENT_EX_DEBUG_
		perror("CEventEx::Init - eventfd");
#endif	// #ifdef _CEVENT_EX_DEBUG_
		return RESULT_ERROR_EVENT_FD;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �C�x���g�t�@�C���f�B�X�N���v�^���擾
//-----------------------------------------------------------------------------
int CEventEx::GetEventFd()
{
	return m_efd;
}


//-----------------------------------------------------------------------------
// �G���[�ԍ����擾
//-----------------------------------------------------------------------------
int CEventEx::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// �C�x���g�ݒ�
//-----------------------------------------------------------------------------
CEventEx::RESULT_ENUM CEventEx::SetEvent()
{
	uint64_t				event = 1;
	int						iRet = 0;


	// �C�x���g�t�@�C���f�B�X�N���v�^�̃`�F�b�N
	if (m_efd == -1)
	{
		return RESULT_ERROR_EVENT_FD;
	}

	// ����������������������������������������������������������
	m_cCounterMutex.Lock();

	// �C�x���g�ݒ�i�C���N�������g�j
	iRet = write(m_efd, &event, sizeof(event));
	if (iRet != sizeof(event))
	{
		m_ErrorNo = errno;
#ifdef _CEVENT_EX_DEBUG_
		perror("CEventEx::SetEvent - write");
#endif	// #ifdef _CEVENT_EX_DEBUG_

		m_cCounterMutex.Unlock();
		return RESULT_ERROR_EVENT_SET;
	}

	// �J�E���^�[�l���C���N�������g
	m_Counter++;

	// ���̏����ɓ������ꍇ�͏��������܂��Ă���Ƃ������ƂȂ̂ŁA�������������Ă�������
	if (m_Counter == MONITORING_COUNTER_NUM)
	{
#ifdef _CEVENT_EX_DEBUG_
		printf("CEventEx::SetEvent - <Warning> Processing has accumulated!\n");
#endif	// #ifdef _CEVENT_EX_DEBUG_
	}

	m_cCounterMutex.Unlock();
	// ����������������������������������������������������������

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �C�x���g�N���A
//-----------------------------------------------------------------------------
CEventEx::RESULT_ENUM CEventEx::ClearEvent()
{
	uint64_t				event = 0;
	int						iRet = 0;
	fd_set					readfds;
	timeval					tTimeval;


	// �C�x���g�t�@�C���f�B�X�N���v�^�̃`�F�b�N
	if (m_efd == -1)
	{
		return RESULT_ERROR_EVENT_FD;
	}

	tTimeval.tv_sec = 0;
	tTimeval.tv_usec = 0;


	// �C�x���g���Z�b�g���s����邩�̃`�F�b�N
	FD_ZERO(&readfds);
	FD_SET(m_efd, &readfds);
	iRet = select(m_efd + 1, &readfds, NULL, NULL, &tTimeval);
	if (iRet < 0)
	{
		m_ErrorNo = errno;
#ifdef _CEVENT_EX_DEBUG_
		perror("CEventEx::ClearEvent - select");
#endif	// #ifdef _CEVENT_EX_DEBUG_
		return RESULT_ERROR_SYSTEM;
	}
	if (iRet == 0)
	{
		// �������Ȃ��i����I���j
		// �����Ƀ��Z�b�g����Ă���Ƃ���ResetEvent���s���ƁA���̃��[�g��ʂ�
	}
	else
	{
		// ����������������������������������������������������������
		m_cCounterMutex.Lock();

		// �ꉞ�n������
		if (m_Counter != 0)
		{
			// �J�E���^�[�l��0�ɂȂ����Ƃ��ɁA�C�x���g�N���A���s��
			m_Counter--;
			if (m_Counter == 0)
			{
				// �C�x���g�N���A
				iRet = read(m_efd, &event, sizeof(event));
				if (iRet < 0)
				{
					m_ErrorNo = errno;
#ifdef _CEVENT_EX_DEBUG_
					perror("CEventEx::ClearEvent - read");
#endif	// #ifdef _CEVENT_EX_DEBUG_

					m_cCounterMutex.Unlock();
					return RESULT_ERROR_EVENT_CLEAR;
				}
			}
		}

		m_cCounterMutex.Unlock();
		// ����������������������������������������������������������
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �C�x���g�҂�
//-----------------------------------------------------------------------------
CEventEx::RESULT_ENUM CEventEx::Wait(unsigned int Timeout)
{
	fd_set			fdRead;
	int				iRet = 0;
	timeval			timeout;


	// �C�x���g�t�@�C���f�B�X�N���v�^�̃`�F�b�N
	if (m_efd == -1)
	{
		return RESULT_ERROR_EVENT_FD;
	}

	// �E�G�C�g����
	FD_ZERO(&fdRead);
	FD_SET(m_efd, &fdRead);
	if (Timeout == 0)
	{
		iRet = select(m_efd + 1, &fdRead, NULL, NULL, NULL);
	}
	else
	{
		timeout.tv_sec = Timeout / 1000;
		timeout.tv_usec = (Timeout % 1000) * 1000;
		iRet = select(m_efd + 1, &fdRead, NULL, NULL, &timeout);
	}

	if (iRet < 0)					// �G���[
	{
		m_ErrorNo = errno;
#ifdef _CEVENT_EX_DEBUG_
		perror("CEventEx::Wait - select");
#endif	// #ifdef _CEVENT_EX_DEBUG_
		return RESULT_ERROR_EVENT_WAIT;
	}
	else if (iRet == 0)				// �^�C���A�E�g
	{
		return RESULT_WAIT_TIMEOUT;
	}

	return RESULT_RECIVE_EVENT;
}