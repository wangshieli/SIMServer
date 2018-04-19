#pragma once

void DoSend(BUFFER_OBJ* bobj);

void DealTail(msgpack::sbuffer& sBuf, BUFFER_OBJ* bobj);