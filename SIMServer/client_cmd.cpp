#include "stdafx.h"
#include <msgpack.hpp>
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
	BUFFER_OBJ* bobj = (BUFFER_OBJ*)_bobj;

	msgpack::unpacker unpack_;
	msgpack::object_handle result_;
	unpack_.reserve_buffer(bobj->dwRecvedCount - 2);
	memcpy_s(unpack_.buffer(), bobj->dwRecvedCount - 2, bobj->data, bobj->dwRecvedCount - 2);
	unpack_.buffer_consumed(bobj->dwRecvedCount - 2);
	unpack_.next(result_);

	msgpack::object* pObj = result_.get().via.array.ptr;
	int nCmd = (pObj++)->as<int>();
	int nSubCmd = (pObj++)->as<int>();
	msgpack::object* pArray = (pObj++)->via.array.ptr;
	msgpack::object* pDataObj = (pArray++)->via.array.ptr;
	std::string strJrhm = (pDataObj++)->as<std::string>();
	std::string dxzh = (pDataObj++)->as<std::string>();

	_tprintf(_T("%d-%d-%s-%s\n"), nCmd, nSubCmd, strJrhm.c_str(), dxzh.c_str());
	const TCHAR* pData = _T("²âÊÔ·µ»Ø");
	memset(bobj->data, 0x00, bobj->datalen);
	bobj->dwRecvedCount = strlen(pData);
	memcpy(bobj->data, pData, bobj->datalen);
	bobj->dwSendedCount = 0;

	SOCKET_OBJ* c_sobj = bobj->pRelatedSObj;
	if (c_sobj->CheckSend(bobj))
	{
		bobj->SetIoRequestFunction(Client_SendCompletionFailed, Client_SendCompletionSuccess);
		if (!CLient_PostSend(c_sobj, bobj))
		{
			Client_SendCompletionFailed(c_sobj, bobj);
		}
	}
}