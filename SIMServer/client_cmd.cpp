#include "stdafx.h"
#include "SignalData.h"
#include "client_cmd.h"
#include "client_RequestPost.h"
#include "client_completion.h"

void Client_CmdCompletionFailed(void* bobj_, void* _bobj)
{
	_tprintf(_T("rewrwer\n"));
}

void Client_CmdCompletionSuccess(DWORD dwTranstion, void* bobj_, void* _bobj)
{
	BUFFER_OBJ* obj = (BUFFER_OBJ*)_bobj;

	_tprintf(_T("Client_CmdCompletionSuccess : %s\n"), obj->data);
	const TCHAR* pData = _T("²âÊÔ·µ»Ø");
	memset(obj->data, 0x00, obj->datalen);
	obj->dwRecvedCount = strlen(pData);
	memcpy(obj->data, pData, obj->datalen);
	obj->dwSendedCount = 0;

	SOCKET_OBJ* c_sobj = obj->pRelatedSObj;
	if (c_sobj->CheckSend(obj))
	{
		obj->SetIoRequestFunction(Client_SendCompletionFailed, Client_SendCompletionSuccess);
		if (!CLient_PostSend(c_sobj, obj))
		{
			Client_SendCompletionFailed(c_sobj, obj);
		}
	}
}