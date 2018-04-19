#include "stdafx.h"
#include "SignalData.h"
#include "mempool.h"
#include "client_RequestPost.h"
#include "client_completion.h"

bool Client_PostAcceptEx(LISTEN_OBJ* lobj, int nIndex)
{
	DWORD dwBytes = 0;
	SOCKET_OBJ* c_sobj = NULL;
	BUFFER_OBJ* c_bobj = NULL;

	c_bobj = allocBObj(g_dwPageSize);
	if (NULL == c_bobj)
		return false;

	c_sobj = allocSObj(g_dwPageSize);
	if (NULL == c_sobj)
	{
		freeBObj(c_bobj);
		return false;
	}

	c_sobj->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == c_sobj->sock)
	{
		freeBObj(c_bobj);
		freeSObj(c_sobj);
		return false;
	}

	c_bobj->pRelatedSObj = c_sobj;
	c_bobj->SetIoRequestFunction(Client_AcceptCompletionFailed, Client_AcceptCompletionSuccess);

	c_sobj->nKey = GetRand();
	lobj->InsertAcpt(c_sobj);

	bool brt = lpfnAccpetEx(lobj->sListenSock,
		c_sobj->sock, c_sobj->buf,
		c_sobj->dwBufsize - ((sizeof(sockaddr_in) + 16) * 2),
		sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &dwBytes, &c_bobj->ol);
	if (!brt)
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			_tprintf(_T("acceptex Ê§°Ü%d\n"), WSAGetLastError());
			lobj->RemoveAcpt(c_sobj);
			CSCloseSocket(c_sobj);
			freeBObj(c_bobj);
			freeSObj(c_sobj);
			return false;
		}
	}

	return true;
}

BOOL Client_PostZeroRecv(SOCKET_OBJ* _sobj, BUFFER_OBJ* _bobj)
{
	DWORD dwBytes = 0,
		dwFlags = 0;

	int err = 0;

	_sobj->wsabuf[0].buf = NULL;
	_sobj->wsabuf[0].len = 0;
	_sobj->wsabuf[1].buf = NULL;
	_sobj->wsabuf[1].len = 0;

	err = WSARecv(_sobj->sock, _sobj->wsabuf, 2, &dwBytes, &dwFlags, &_bobj->ol, NULL);
	if (SOCKET_ERROR == err && WSA_IO_PENDING != WSAGetLastError())
		return FALSE;

	return TRUE;
}

BOOL Client_PostRecv(SOCKET_OBJ* _sobj, BUFFER_OBJ* _bobj)
{
	DWORD dwBytes = 0,
		dwFlags = 0;

	int err = 0;

	_sobj->InitWSABUFS();

	err = WSARecv(_sobj->sock, _sobj->wsabuf, 2, &dwBytes, &dwFlags, &_bobj->ol, NULL);
	if (SOCKET_ERROR == err && WSA_IO_PENDING != WSAGetLastError())
		return FALSE;

	return TRUE;
}

BOOL CLient_PostSend(SOCKET_OBJ* _sobj, BUFFER_OBJ* _bobj)
{
	DWORD dwBytes = 0;
	int err = 0;

	_bobj->wsaBuf.buf = _bobj->data + _bobj->dwSendedCount;
	_bobj->wsaBuf.len = _bobj->dwRecvedCount - _bobj->dwSendedCount;

	err = WSASend(_sobj->sock, &_bobj->wsaBuf, 1, &dwBytes, 0, &_bobj->ol, NULL);
	if (SOCKET_ERROR == err && WSA_IO_PENDING != WSAGetLastError())
		return FALSE;

	return TRUE;
}