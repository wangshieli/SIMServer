#pragma once

bool Client_PostAcceptEx(LISTEN_OBJ* lobj, int nIndex);

BOOL Client_PostZeroRecv(SOCKET_OBJ* _sobj, BUFFER_OBJ* _bobj);

BOOL Client_PostRecv(SOCKET_OBJ* _sobj, BUFFER_OBJ* _bobj);

BOOL CLient_PostSend(SOCKET_OBJ* _sobj, BUFFER_OBJ* _bobj);