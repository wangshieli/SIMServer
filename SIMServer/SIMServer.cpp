// SIMServer.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "global_init.h"
#include <process.h>
#include "SignalData.h"
#include "client_RequestPost.h"
#include "mempool.h"

#ifdef _DEBUG
#define SERVER_PORT	6099
#define STERN_PORT	6666
#else
#define SERVER_PORT 6086
#define STERN_PORT	6666
#endif // _DEBUG

#define POST_COUNT			10
#define MAX_PENDING_ACCEPT	200

LISTEN_OBJ* LSock_Array[WSA_MAXIMUM_WAIT_EVENTS];
HANDLE hEvent_Array[WSA_MAXIMUM_WAIT_EVENTS];
DWORD g_dwListenPortCount = 0;

bool InitListenSObj(LISTEN_OBJ **lobj, USHORT nPort);

unsigned int _stdcall test(LPVOID pVoid);



int main()
{
	Initmempool();
	if (!InitWinsock2() || !GetExtensionFunctionPointer())
		return -1;

	SYSTEM_INFO sys;
	GetSystemInfo(&sys);
	DWORD dwNumberOfCPU = sys.dwNumberOfProcessors * 2;
	g_dwPageSize = sys.dwPageSize * 8;

	hCompPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, 0);
	if (NULL == hCompPort)
	{
		_tprintf(_T("创建完成端口失败, errCode = %d\n"), WSAGetLastError());
		WSACleanup();
		return -1;
	}

	int nThreadCount = 0;
	for (DWORD i = 0; i < dwNumberOfCPU; i++)
	{
		HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, test, NULL, 0, NULL);
		if (NULL != hCompPort)
		{
			CloseHandle(hThread);
			nThreadCount++;
		}
	}

	LISTEN_OBJ *lobj = NULL;
	if (!InitListenSObj(&lobj, SERVER_PORT))
	{
		return -1;
	}

	for (size_t i = 0; i < POST_COUNT; i++)
	{
		Client_PostAcceptEx(LSock_Array[0], 0);
	}

	while (true)
	{
		int rt = WSAWaitForMultipleEvents(g_dwListenPortCount, hEvent_Array, FALSE, INFINITE, FALSE);
		if (WSA_WAIT_FAILED == rt)
		{
			_tprintf(_T("带来异常，推出\n"));
			return -1;
		}

		for (DWORD nIndex = 0; nIndex < g_dwListenPortCount; nIndex++)
		{
			rt = WaitForSingleObject(hEvent_Array[nIndex], 0);
			if (WSA_WAIT_TIMEOUT == rt)
				continue;
			else if (WSA_WAIT_FAILED == rt)
			{
				_tprintf(_T("带来异常，推出\n"));
				return -1;
			}

			WSAResetEvent(hEvent_Array[nIndex]);
			for (size_t i = 0; i < POST_COUNT; i++)
			{
				Client_PostAcceptEx(LSock_Array[nIndex], nIndex);
			}

			if (LSock_Array[nIndex]->AcptMap.size() > MAX_PENDING_ACCEPT)
			{
				int nError = 0;
				int optlen,
					optval;
				optlen = sizeof(optval);

				for (tbb::concurrent_hash_map<int, SOCKET_OBJ*>::iterator i = LSock_Array[nIndex]->AcptMap.begin(); i != LSock_Array[nIndex]->AcptMap.end(); ++i)
				{
					nError = getsockopt(i->second->sock, SOL_SOCKET, SO_CONNECT_TIME, (char*)&optval, &optlen);
					if (SOCKET_ERROR == nError)
					{
						_tprintf(_T("getsockopt failed error: %d\n"), WSAGetLastError());
						continue;
					}

					if (optval != 0xffffffff && optval > 60)
						CSCloseSocket(i->second);
				}
			}
		}
	}

	system("pause");
    return 0;
}

bool InitListenSObj(LISTEN_OBJ **lobj, USHORT nPort)
{
	*lobj = new LISTEN_OBJ();
	if (NULL == (*lobj))
	{
		_tprintf(_T("分配监听socket对象失败, errCode = %d\n"), WSAGetLastError());
		CloseHandle(hCompPort);
		WSACleanup();
		return false;
	}
	(*lobj)->init();

	(*lobj)->sListenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == (*lobj)->sListenSock)
	{
		_tprintf(_T("创建监听socket失败, errCode = %d\n"), WSAGetLastError());
		goto error;
	}

	struct sockaddr_in laddr;
	memset(&laddr, 0x00, sizeof(laddr));
	laddr.sin_family = AF_INET;
	laddr.sin_port = htons(nPort);
	laddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (SOCKET_ERROR == bind((*lobj)->sListenSock, (SOCKADDR*)&laddr, sizeof(laddr)))
	{
		_tprintf(_T("监听socket绑定失败, errCode = %d\n"), WSAGetLastError());
		goto error;
	}

	// 创建事件，完成自动处理资源分配
	//(*lobj)->hPostAcceptExEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	(*lobj)->hPostAcceptExEvent = WSACreateEvent();
	if (NULL == (*lobj)->hPostAcceptExEvent)
	{
		_tprintf(_T("监听事件创建失败, errCode = %d\n"), WSAGetLastError());
		goto error;
	}

	LSock_Array[g_dwListenPortCount] = (*lobj);
	hEvent_Array[g_dwListenPortCount++] = (*lobj)->hPostAcceptExEvent;

	if (SOCKET_ERROR == WSAEventSelect((*lobj)->sListenSock, (*lobj)->hPostAcceptExEvent, FD_ACCEPT))
	{
		_tprintf(_T("监听事件关联失败, errCode = %d\n"), WSAGetLastError());
		goto error;
	}

	if (NULL == CreateIoCompletionPort((HANDLE)(*lobj)->sListenSock, hCompPort, (ULONG_PTR)(*lobj), 0))
	{
		_tprintf(_T("监听socket关联完成端口失败, errCode = %d\n"), WSAGetLastError());
		goto error;
	}

	if (SOCKET_ERROR == listen((*lobj)->sListenSock, SOMAXCONN))
	{
		_tprintf(_T("socket监听失败, errCode = %d\n"), WSAGetLastError());
		goto error;
	}

	return true;
error:
	delete (*lobj);
	CloseHandle(hCompPort);
	WSACleanup();
	return false;
}

unsigned int _stdcall test(LPVOID pVoid)
{
	ULONG_PTR key;
	BUFFER_OBJ* bobj;
	LPOVERLAPPED lpol;
	DWORD dwTranstion;
	BOOL bSuccess;

	while (true)
	{
		bSuccess = GetQueuedCompletionStatus(hCompPort, &dwTranstion, &key, &lpol, INFINITE);
		if (NULL == lpol)
		{
			printf("如果不是认为设定NULL为退出信号，那么就是发生重大错误，直接退出\n");
			return 0;
		}

		bobj = CONTAINING_RECORD(lpol, BUFFER_OBJ, ol);

		if (!bSuccess)
			bobj->pfnFailed((void*)key, bobj);
		else
			bobj->pfnSuccess(dwTranstion, (void*)key, bobj);
	}

	return 0;
}