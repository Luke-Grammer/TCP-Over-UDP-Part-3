// Properties.h
// CSCE 463-500
// Luke Grammer
// 11/21/19

#pragma once

struct Properties
{
	SOCKET sock;
	struct sockaddr_in server;
	struct in_addr serverAddr;

	CRITICAL_SECTION criticalSection;
	HANDLE eventQuit = CreateEvent(NULL, true, false, NULL);
	HANDLE ControlAckReceived = CreateEvent(NULL, false, false, NULL);
	HANDLE socketReceiveReady = CreateEvent(NULL, false, false, NULL);
	HANDLE workerException    = CreateEvent(NULL, false, false, NULL);
	HANDLE empty, full;
	DWORD goodput = 0;
	DWORD senderBase = 0;
	DWORD windowSize = 0;
	UINT64 bytesAcked = 0;
	DWORD timeoutPackets = 0;
	DWORD fastRetxPackets = 0;
	DWORD nextToSend = 0;
	INT estRTT = 0;
	INT devRTT = 0;
	DWORD RTO = 0;
	Timer totalTimer;
	INT errorCode = -1;
	
	DWORD numPendingPackets = 0;
	Packet* pendingPackets;
	
	Properties(DWORD senderWindow)
	{
		InitializeCriticalSection(&criticalSection);
		empty = CreateSemaphore(NULL, 1, senderWindow, NULL);
		full = CreateSemaphore(NULL, 0, senderWindow, NULL);
		
		pendingPackets = new Packet[senderWindow];
		memset(pendingPackets, 0, sizeof(Packet) * senderWindow);
	}
	
	~Properties() 
	{ 
		for (int i = 0; i < windowSize; i++) {
			if (pendingPackets[i].buf != NULL)
				delete[] pendingPackets[i].buf;
		}
		delete[] pendingPackets;
		DeleteCriticalSection(&criticalSection); 
	}
};
