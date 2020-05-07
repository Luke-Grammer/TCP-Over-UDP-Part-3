// SenderWorker.h
// CSCE 463-500
// Luke Grammer
// 11/21/19

#pragma once

struct SenderWorker 
{
	static VOID RunWorker(LPVOID properties);

	static VOID ReceiveACK(char* responseBuffer, Properties* p);

	static VOID RetransmitBase(Properties* properties);
};