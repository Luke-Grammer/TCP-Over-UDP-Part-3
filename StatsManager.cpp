// StatsManager.cpp
// CSCE 463-500
// Luke Grammer
// 11/21/19

#include "pch.h"

using namespace std;

// print running statistics for webcrawling worker threads at a fixed 2s interval
void StatsManager::PrintStats(LPVOID properties)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	Properties* p = (Properties*)properties;
	Timer segmentTimer;

	// until shutdown request has been sent by main
	while (WaitForSingleObject(p->eventQuit, STATS_INTERVAL * 1000) == WAIT_TIMEOUT)
	{
		segmentTimer.Stop();

		// print statistics
		printf("[%2d] B %6d (%5.1f MB) N %6d T %d F %d W %d S %0.3f Mbps RTT %5.3f\n",
			(INT) p->totalTimer.ElapsedSeconds(), 
			p->senderBase, p->bytesAcked / 1000000.0, (DWORD) (p->nextToSend), p->timeoutPackets, 
			p->fastRetxPackets, p->windowSize, ((UINT64) p->goodput * 8) / (1000.0 * segmentTimer.ElapsedMilliseconds()), 
			p->estRTT / 1000.0);

		// reset goodput
		InterlockedExchange((volatile DWORD*)&p->goodput, 0);

		segmentTimer.Start();
	}
}