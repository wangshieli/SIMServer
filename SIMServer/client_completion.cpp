#include "stdafx.h"
#include "SignalData.h"
#include "client_completion.h"
#include "mempool.h"
#include "client_RequestPost.h"
#include "client_cmd.h"

struct tcp_keepalive alive_in = { TRUE, 1000 * 10, 1000 };
struct tcp_keepalive alive_out = { 0 };
unsigned long ulBytesReturn = 0;

void Client_AcceptCompletionFailed(void* _lobj, void* _c_bobj)
{
	LISTEN_OBJ* lobj = (LISTEN_OBJ*)_lobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_c_bobj;
	SOCKET_OBJ* c_sobj = c_bobj->pRelatedSObj;

	lobj->RemoveAcpt(c_sobj);
	
	CSCloseSocket(c_sobj);
	freeSObj(c_sobj);
	freeBObj(c_bobj);
}

void Client_AcceptCompletionSuccess(DWORD dwTranstion, void* _lobj, void* _c_bobj)
{
	if (dwTranstion <= 0)
		return Client_AcceptCompletionFailed(_lobj, _c_bobj);

	LISTEN_OBJ* lobj = (LISTEN_OBJ*)_lobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_c_bobj;
	SOCKET_OBJ* c_sobj = c_bobj->pRelatedSObj;

	lobj->RemoveAcpt(c_sobj);

	if (SOCKET_ERROR == WSAIoctl(c_sobj->sock, SIO_KEEPALIVE_VALS, &alive_in, sizeof(alive_in),
		&alive_out, sizeof(alive_out), &ulBytesReturn, NULL, NULL))
		_tprintf(_T("设置客户端连接心跳检测失败, errCode = %d\n"), WSAGetLastError());

	if (NULL == CreateIoCompletionPort((HANDLE)c_sobj->sock, hCompPort, (ULONG_PTR)c_sobj, 0))
	{
		_tprintf(_T("客户端socket绑定完成端口失败, errCode = %d\n"), WSAGetLastError());
		CSCloseSocket(c_sobj);
		freeSObj(c_sobj);
		freeBObj(c_bobj);
		return;
	}

	SOCKADDR* localAddr,
		*remoteAddr;
	localAddr = NULL;
	remoteAddr = NULL;
	int localAddrlen,
		remoteAddrlen;

	lpfnGetAcceptExSockaddrs(c_sobj->buf, c_sobj->dwBufsize - ((sizeof(sockaddr_in) + 16) * 2),
		sizeof(sockaddr_in) + 16,
		sizeof(sockaddr_in) + 16,
		&localAddr, &localAddrlen,
		&remoteAddr, &remoteAddrlen);

	c_sobj->InitWRpos(dwTranstion);
	DWORD len = c_sobj->GetCmdDataLength();
	while (len)
	{
		BUFFER_OBJ* obj = allocBObj(g_dwPageSize);
		c_sobj->Read(obj->data, len);
		obj->dwRecvedCount = len;
		obj->pRelatedSObj = c_sobj;
		c_sobj->AddRef();
		obj->SetIoRequestFunction(Client_CmdCompletionFailed, Client_CmdCompletionSuccess);
		if (0 == PostQueuedCompletionStatus(hCompPort, len, (ULONG_PTR)obj, &obj->ol))
		{
			_tprintf(_T("%d\n"), WSAGetLastError());
			c_sobj->DecRef();
			freeBObj(obj);
		}
		len = c_sobj->GetCmdDataLength();
	}

	c_bobj->SetIoRequestFunction(Client_ZeroRecvCompletionFailed, Client_ZeroRecvCompletionSuccess);
	if (!Client_PostZeroRecv(c_sobj, c_bobj))
	{
		// 考虑同步释放问题
		if (0 == InterlockedDecrement(&c_sobj->nRef))
		{
			CSCloseSocket(c_sobj);
			freeSObj(c_sobj);
		}
		freeBObj(c_bobj);
	}
}

void Client_ZeroRecvCompletionFailed(void* _sobj, void* _bobj)
{
	SOCKET_OBJ* c_sobj = (SOCKET_OBJ*)_sobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_bobj;

	// 考虑同步释放问题
	if (0 == InterlockedDecrement(&c_sobj->nRef))
	{
		CSCloseSocket(c_sobj);
		freeSObj(c_sobj);
	}

	freeBObj(c_bobj);
}

void Client_ZeroRecvCompletionSuccess(DWORD dwTranstion, void* _sobj, void* _bobj)
{
	SOCKET_OBJ* c_sobj = (SOCKET_OBJ*)_sobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_bobj;

	c_bobj->SetIoRequestFunction(Client_RecvCompletionFailed, Client_RecvCompletionSuccess);
	if (!Client_PostRecv(c_sobj, c_bobj))
	{
		if (0 == InterlockedDecrement(&c_sobj->nRef))
		{
			CSCloseSocket(c_sobj);
			freeSObj(c_sobj);
		}
		freeBObj(c_bobj);
	}
}

void Client_RecvCompletionFailed(void* _sobj, void* _bobj)
{
	SOCKET_OBJ* c_sobj = (SOCKET_OBJ*)_sobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_bobj;

	// 考虑同步释放问题
	if (0 == InterlockedDecrement(&c_sobj->nRef))
	{
		CSCloseSocket(c_sobj);
		freeSObj(c_sobj);
	}
	freeBObj(c_bobj);
}

void Client_RecvCompletionSuccess(DWORD dwTranstion, void* _sobj, void* _bobj)
{
	if (dwTranstion <= 0)
		return Client_RecvCompletionFailed(_sobj, _bobj);

	SOCKET_OBJ* c_sobj = (SOCKET_OBJ*)_sobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_bobj;

	c_sobj->InitWRpos(dwTranstion);
	DWORD len = c_sobj->GetCmdDataLength();
	while (len)
	{
		BUFFER_OBJ* obj = allocBObj(g_dwPageSize);
		c_sobj->Read(obj->data, len);
		obj->dwRecvedCount = len;
		obj->pRelatedSObj = c_sobj;
		c_sobj->AddRef();
		obj->SetIoRequestFunction(Client_CmdCompletionFailed, Client_CmdCompletionSuccess);
		if (0 == PostQueuedCompletionStatus(hCompPort, len, (ULONG_PTR)obj, &obj->ol))
		{
			_tprintf(_T("%d\n"), WSAGetLastError());
			freeBObj(obj);
			if (0 == InterlockedDecrement(&c_sobj->nRef))
			{
				CSCloseSocket(c_sobj);
				freeSObj(c_sobj);
				return;
			}
		}

		len = c_sobj->GetCmdDataLength();
	}

	c_bobj->SetIoRequestFunction(Client_ZeroRecvCompletionFailed, Client_ZeroRecvCompletionSuccess);
	if (!Client_PostZeroRecv(c_sobj, c_bobj))
	{
		// 考虑同步释放问题
		if (0 == InterlockedDecrement(&c_sobj->nRef))
		{
			CSCloseSocket(c_sobj);
			freeSObj(c_sobj);
		}
		freeBObj(c_bobj);
	}
}

void Client_SendCompletionFailed(void* _sobj, void* _bobj)
{
	SOCKET_OBJ* c_sobj = (SOCKET_OBJ*)_sobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_bobj;

	BUFFER_OBJ* obj = c_sobj->GetNextData();
	freeBObj(c_bobj);
	if (obj)
	{
		InterlockedDecrement(&c_sobj->nRef);
		obj->SetIoRequestFunction(Client_SendCompletionFailed, Client_SendCompletionSuccess);
		if (!CLient_PostSend(c_sobj, obj))
		{
			Client_SendCompletionFailed(c_sobj, obj);
		}
	}
	else
	{
		if (0 == InterlockedDecrement(&c_sobj->nRef))
		{
			CSCloseSocket(c_sobj);
			freeSObj(c_sobj);
		}
	}

	//if (0 == InterlockedDecrement(&c_sobj->nRef))
	//{
	//	c_sobj->SendOK(c_bobj);
	//	freeBObj(c_bobj);
	//	CSCloseSocket(c_sobj);
	//	freeSObj(c_sobj);
	//}
	//else
	//{
	//	BUFFER_OBJ* obj = c_sobj->GetNextData();
	//	freeBObj(c_bobj);
	//	if (obj)
	//	{
	//		obj->SetIoRequestFunction(Client_SendCompletionFailed, Client_SendCompletionSuccess);
	//		if (!CLient_PostSend(c_sobj, obj))
	//		{
	//			Client_SendCompletionFailed(c_sobj, obj);
	//		}
	//	}
	//}
}
void Client_SendCompletionSuccess(DWORD dwTranstion, void* _sobj, void* _bobj)
{
	if (dwTranstion <= 0)
		return Client_SendCompletionFailed(_sobj, _bobj);

	SOCKET_OBJ* c_sobj = (SOCKET_OBJ*)_sobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_bobj;

	c_bobj->dwSendedCount += dwTranstion;
	if (c_bobj->dwSendedCount < c_bobj->dwRecvedCount)
	{
		if (!CLient_PostSend(c_sobj, c_bobj))
		{
			Client_SendCompletionFailed(c_sobj, c_bobj);
			return;
		}
		return;
	}

	BUFFER_OBJ* obj = c_sobj->GetNextData();
	freeBObj(c_bobj);
	if (obj)
	{
		InterlockedDecrement(&c_sobj->nRef);
		obj->SetIoRequestFunction(Client_SendCompletionFailed, Client_SendCompletionSuccess);
		if (!CLient_PostSend(c_sobj, obj))
		{
			Client_SendCompletionFailed(c_sobj, obj);
		}
	}
	else
	{
		if (0 == InterlockedDecrement(&c_sobj->nRef))
		{
			CSCloseSocket(c_sobj);
			freeSObj(c_sobj);
		}
	}

	//c_sobj->SendOK(c_bobj);
	//freeBObj(c_bobj);
	//
	//if (0 == InterlockedDecrement(&c_sobj->nRef))
	//{
	//	CSCloseSocket(c_sobj);
	//	freeSObj(c_sobj);
	//}
	//else
	//{
	//	BUFFER_OBJ* obj = c_sobj->GetNextData();
	//	if (obj)
	//	{
	//		obj->SetIoRequestFunction(Client_SendCompletionFailed, Client_SendCompletionSuccess);
	//		if (!CLient_PostSend(c_sobj, obj))
	//		{
	//			Client_SendCompletionFailed(c_sobj, obj);
	//		}
	//	}
	//}
}