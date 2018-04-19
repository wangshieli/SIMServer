#pragma once
#include <tbb\concurrent_hash_map.h>
#include <iostream>
#include <string>
#include <list>

//using namespace tbb;
//using namespace std;

typedef void(*PTIoRequestSuccess)(DWORD dwTranstion, void* key, void* buf);
typedef void(*PTIoRequestFailed)(void* key, void* buf);

typedef struct _buffer_obj
{
public:
	WSAOVERLAPPED ol;
	PTIoRequestFailed pfnFailed;
	PTIoRequestSuccess pfnSuccess;
	struct _socket_obj* pRelatedSObj;
	WSABUF wsaBuf;
	DWORD dwRecvedCount;
	DWORD dwSendedCount;
	int datalen;
	TCHAR data[1];

public:
	void init(DWORD usefull_space)
	{
		memset(&ol, 0x00, sizeof(ol));
		pfnFailed = NULL;
		pfnSuccess = NULL;
		pRelatedSObj = NULL;
		dwRecvedCount = 0;
		dwSendedCount = 0;
		datalen = usefull_space;
		memset(data, 0x00, usefull_space);
	}

	void SetIoRequestFunction(PTIoRequestFailed _pfnFailed, PTIoRequestSuccess _pfnSuccess)
	{
		pfnFailed = _pfnFailed;
		pfnSuccess = _pfnSuccess;
	}

}BUFFER_OBJ;
#define SIZE_OF_BUFFER_OBJ sizeof(BUFFER_OBJ)

typedef struct _buffer_obj_t
{
public:
	WSAOVERLAPPED ol;
	PTIoRequestFailed pfnFailed;
	PTIoRequestSuccess pfnSuccess;
	struct _socket_obj* pRelatedSObj;
	WSABUF wsaBuf;
	DWORD dwRecvedCount;
	DWORD dwSendedCount;
	int datalen;
	//TCHAR data[1];
}BUFFER_OBJ_T;
#define SIZE_OF_BUFFER_OBJ_T sizeof(BUFFER_OBJ_T)

typedef struct _socket_obj
{
	SOCKET sock;
	std::list<BUFFER_OBJ*>* lstSend;
	CRITICAL_SECTION cs;
	WSABUF wsabuf[2];
	DWORD dwBufsize;
	DWORD dwWritepos;
	DWORD dwReadpos;
	DWORD dwRightsize;
	DWORD dwLeftsize;
	int nKey;
	bool bFull;
	bool bEmpty;
	volatile long nRef;
	TCHAR buf[1];

	void Init(DWORD _Bufsize)
	{
		if (INVALID_SOCKET != sock)
		{
			closesocket(sock);
			sock = INVALID_SOCKET;
		}
		dwBufsize = _Bufsize;
		dwWritepos = 0;
		dwReadpos = 0;
		dwRightsize = 0;
		dwLeftsize = 0;
		nKey = 0;
		bFull = false;
		bEmpty = true;
		nRef = 1;
		InitializeCriticalSection(&cs);
		lstSend->clear();
	}

	void Clear()
	{
		if (NULL != lstSend)
			delete lstSend;
	}

	void InitRLsize()
	{
		if (bFull)
		{
			dwRightsize = 0;
			dwLeftsize = 0;
		}
		else if (dwReadpos <= dwWritepos)
		{
			dwRightsize = dwBufsize - dwWritepos;
			dwLeftsize = dwReadpos;
		}
		else
		{
			dwRightsize = dwReadpos - dwWritepos;
			dwLeftsize = 0;
		}
	}

	void InitWSABUFS()
	{
		InitRLsize();
		wsabuf[0].buf = buf + dwWritepos;
		wsabuf[0].len = dwRightsize;
		wsabuf[1].buf = buf;
		wsabuf[1].len = dwLeftsize;
	}

	void InitWRpos(DWORD dwTranstion)
	{
		if (dwTranstion <= 0)
			return;

		if (bFull)
			return;

		dwWritepos = (dwTranstion > dwRightsize) ? (dwTranstion - dwRightsize) : (dwWritepos + dwTranstion);
		bFull = (dwWritepos == dwReadpos);
		bEmpty = false;
	}

	DWORD GetCmdDataLength()
	{
		if (bEmpty)
			return 0;

		DWORD dwNum = 0;
		if (dwWritepos > dwReadpos)
		{
			for (DWORD i = dwReadpos; i < dwWritepos; i++)
			{
				++dwNum;
				if (buf[i] == '\n')
				{
					return dwNum;
				}
			}
		}
		else
		{
			for (DWORD i = dwReadpos; i < dwBufsize; i++)
			{
				++dwNum;
				if (buf[i] == '\n')
				{
					return dwNum;
				}
			}
			for (DWORD i = 0; i < dwWritepos; i++)
			{
				++dwNum;
				if (buf[i] == '\n')
				{
					return dwNum;
				}
			}
		}
		return 0;
	}

	int Read(TCHAR* _buf, DWORD dwNum)
	{
		if (dwNum <= 0)
			return 0;

		if (bEmpty)
			return 0;

		bFull = false;
		if (dwReadpos < dwWritepos)
		{
			memcpy(_buf, buf + dwReadpos, dwNum);
			dwReadpos += dwNum;
			bEmpty = (dwReadpos == dwWritepos);
			return dwNum;
		}
		else if (dwReadpos >= dwWritepos)
		{
			DWORD leftcount = dwBufsize - dwReadpos;
			if (leftcount > dwNum)
			{
				memcpy(_buf, buf + dwReadpos, dwNum);
				dwReadpos += dwNum;
				bEmpty = (dwReadpos == dwWritepos);
				return dwNum;
			}
			else
			{
				memcpy(_buf, buf + dwReadpos, leftcount);
				dwReadpos = dwNum - leftcount;
				memcpy(_buf + leftcount, buf, dwReadpos);
				bEmpty = (dwReadpos == dwWritepos);
				return leftcount + dwReadpos;
			}
		}

		return 0;
	}

	void AddRef()
	{
		InterlockedIncrement(&nRef);
	}

	void DecRef()
	{
		InterlockedDecrement(&nRef);
	}

	bool CheckSend(BUFFER_OBJ* data)
	{
		EnterCriticalSection(&cs);
		if (lstSend->empty())
		{
			lstSend->push_front(data);
			LeaveCriticalSection(&cs);
			return true;
		}
		else
		{
			lstSend->push_front(data);
			LeaveCriticalSection(&cs);
			return false;
		}
	}

	void SendOK(BUFFER_OBJ* obj)
	{

		EnterCriticalSection(&cs);
		lstSend->pop_back();
		LeaveCriticalSection(&cs);
	}

	BUFFER_OBJ* GetNextData()
	{
		BUFFER_OBJ* data = NULL;;
		EnterCriticalSection(&cs);
		lstSend->pop_back();
		if (!lstSend->empty())
			data = lstSend->back();
		LeaveCriticalSection(&cs);
		return data;
	}
}SOCKET_OBJ;
#define SIZE_OF_SOCKET_OBJ sizeof(SOCKET_OBJ)

typedef struct _socket_obj_t
{
	SOCKET sock;
	std::list<BUFFER_OBJ*>* lstSend;
	CRITICAL_SECTION cs;
	WSABUF wsabuf[2];
	DWORD dwBufsize;
	DWORD dwWritepos;
	DWORD dwReadpos;
	DWORD dwRightsize;
	DWORD dwLeftsize;
	int nKey;
	bool bFull;
	bool bEmpty;
	volatile long nRef;
	//TCHAR buf[1];
}SOCKET_OBJ_T;
#define SIZE_OF_SOCKET_OBJ_T sizeof(SOCKET_OBJ_T)

typedef struct _listen_obj
{
public:
	SOCKET sListenSock;
	HANDLE hPostAcceptExEvent;

	tbb::concurrent_hash_map<int, SOCKET_OBJ*> AcptMap;

	~_listen_obj()
	{
		if (INVALID_SOCKET != sListenSock)
		{
			closesocket(sListenSock);
			sListenSock = INVALID_SOCKET;
			if (NULL != hPostAcceptExEvent)
			{
				CloseHandle(hPostAcceptExEvent);
				hPostAcceptExEvent = NULL;
			}
		}
	}
public:
	void init()
	{
		sListenSock = INVALID_SOCKET;
		hPostAcceptExEvent = NULL;
		AcptMap.clear();
	}

	void InsertAcpt(SOCKET_OBJ* sobj)
	{
		tbb::concurrent_hash_map<int, SOCKET_OBJ*>::accessor a_ad;
		AcptMap.insert(a_ad, sobj->nKey);
		a_ad->second = sobj;
	}

	void RemoveAcpt(SOCKET_OBJ* sobj)
	{
		tbb::concurrent_hash_map<int, SOCKET_OBJ*>::accessor a_rm;
		if (AcptMap.find(a_rm, sobj->nKey))
			AcptMap.erase(a_rm);
	}
}LISTEN_OBJ;

extern HANDLE hCompPort;
extern DWORD g_dwPageSize;

extern LPFN_ACCEPTEX lpfnAccpetEx;
extern LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs;
extern LPFN_CONNECTEX lpfnConnectEx;

int GetRand();

void CSCloseSocket(SOCKET_OBJ* _sobj);