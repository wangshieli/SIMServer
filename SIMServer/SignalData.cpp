#include "stdafx.h"
#include "SignalData.h"
#include <random>

std::random_device randdev;
std::default_random_engine randeg(randdev());
std::uniform_int_distribution<int> urand(0);

int GetRand()
{
	return urand(randeg);
}

int error_info(BUFFER_OBJ* buffer, const TCHAR* format, ...)
{
	va_list va;
	va_start(va, format);
	int result = _vsntprintf_s(buffer->data, buffer->datalen, _TRUNCATE, format, va);

	va_end(va);
	return result;
}

void CSCloseSocket(SOCKET_OBJ* _sobj)
{
	if (INVALID_SOCKET != _sobj->sock)
	{
		closesocket(_sobj->sock);
		_sobj->sock = INVALID_SOCKET;
	}
}