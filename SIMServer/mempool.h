#pragma once

void Initmempool();

SOCKET_OBJ* allocSObj(DWORD nSize);

void freeSObj(SOCKET_OBJ* obj);

BUFFER_OBJ* allocBObj(DWORD nSize);

void freeBObj(BUFFER_OBJ* obj);