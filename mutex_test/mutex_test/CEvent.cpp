//*****************************************************************************
// Event�N���X
//*****************************************************************************
#include "CEvent.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


#define _CEVENT_DEBUG_


//-----------------------------------------------------------------------------
// �R���X�g���N�^
//-----------------------------------------------------------------------------
CEvent::CEvent()
{
	m_ErrorNo = 0;
	m_efd = -1;
}


//-----------------------------------------------------------------------------
// �f�X�g���N�^
//-----------------------------------------------------------------------------
CEvent::~CEvent()
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
CEvent::RESULT_ENUM CEvent::Init()
{
	// �C�x���g�t�@�C���f�B�X�N���v�^�𐶐�
	m_efd = eventfd(0, 0);
	if (m_efd == -1)
	{
		m_ErrorNo = errno;
#ifdef _CEVENT_DEBUG_
		perror("CEvent - eventfd");
#endif	// #ifdef _CEVENT_DEBUG_
		return RESULT_ERROR_EVENT_FD;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �C�x���g�t�@�C���f�B�X�N���v�^���擾
//-----------------------------------------------------------------------------
int CEvent::GetEventFd()
{
	return m_efd;
}


//-----------------------------------------------------------------------------
// �G���[�ԍ����擾
//-----------------------------------------------------------------------------
int CEvent::GetErrorNo()
{
	return m_ErrorNo;
}


//-----------------------------------------------------------------------------
// �C�x���g�ݒ�
//-----------------------------------------------------------------------------
CEvent::RESULT_ENUM CEvent::SetEvent()
{
	uint64_t				event = 1;
	int						iRet = 0;


	// �C�x���g�t�@�C���f�B�X�N���v�^�̃`�F�b�N
	if (m_efd == -1)
	{
		return RESULT_ERROR_EVENT_FD;
	}

	// �C�x���g�ݒ�
	iRet = write(m_efd, &event, sizeof(event));
	if (iRet != sizeof(event))
	{
		m_ErrorNo = errno;
#ifdef _CEVENT_DEBUG_
		perror("CEvent - write");
#endif	// #ifdef _CEVENT_DEBUG_
		return RESULT_ERROR_EVENT_SET;
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �C�x���g���Z�b�g
//-----------------------------------------------------------------------------
CEvent::RESULT_ENUM CEvent::ResetEvent()
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
#ifdef _CEVENT_DEBUG_
		perror("CEvent - select");
#endif	// #ifdef _CEVENT_DEBUG_
		return RESULT_ERROR_SYSTEM;
	}
	if (iRet == 0)
	{
		// �������Ȃ��i����I���j
		// �����Ƀ��Z�b�g����Ă���Ƃ���ResetEvent���s���ƁA���̃��[�g��ʂ�
	}
	else
	{
		// �C�x���g���Z�b�g
		iRet = read(m_efd, &event, sizeof(event));
		if (iRet < 0)
		{
			m_ErrorNo = errno;
#ifdef _CEVENT_DEBUG_
			perror("CEvent - read");
#endif	// #ifdef _CEVENT_DEBUG_
			return RESULT_ERROR_EVENT_RESET;
		}
	}

	return RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
// �C�x���g�҂�
//-----------------------------------------------------------------------------
CEvent::RESULT_ENUM CEvent::Wait(uint32_t dwTimeout)
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
	if (dwTimeout == 0)
	{
		iRet = select(m_efd + 1, &fdRead, NULL, NULL, NULL);
	}
	else
	{
		timeout.tv_sec = dwTimeout / 1000;
		timeout.tv_usec = (dwTimeout % 1000) * 1000;
		iRet = select(m_efd + 1, &fdRead, NULL, NULL, &timeout);
	}

	if (iRet < 0)					// �G���[
	{
		m_ErrorNo = errno;
		return RESULT_ERROR_EVENT_WAIT;
	}
	else if (iRet == 0)				// �^�C���A�E�g
	{
		return RESULT_WAIT_TIMEOUT;
	}

	return RESULT_RECIVE_EVENT;
}