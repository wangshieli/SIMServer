#pragma once

void Client_AcceptCompletionFailed(void* _lobj, void* _c_bobj);
void Client_AcceptCompletionSuccess(DWORD dwTranstion, void* _lobj, void* _c_bobj);

void Client_ZeroRecvCompletionFailed(void* _lobj, void* _c_bobj);
void Client_ZeroRecvCompletionSuccess(DWORD dwTranstion, void* _lobj, void* _c_bobj);

void Client_RecvCompletionFailed(void* _lobj, void* _c_bobj);
void Client_RecvCompletionSuccess(DWORD dwTranstion, void* _lobj, void* _c_bobj);

void Client_SendCompletionFailed(void* _lobj, void* _c_bobj);
void Client_SendCompletionSuccess(DWORD dwTranstion, void* _lobj, void* _c_bobj);