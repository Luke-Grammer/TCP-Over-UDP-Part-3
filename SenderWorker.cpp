// SenderWorker.cpp
// CSCE 463-500
// Luke Grammer
// 11/21/19

#include "pch.h"

INT result = -1;
DWORD duplicateACKVal = 0;
SHORT numDuplicateACKS = 0;
BOOL resetTimer = false;
DWORD timerExpire = 0;
INT lastReleased = 0;
ReceiverHeader ACKHeader;
clock_t elapsedTime;
Packet* AckPacket;
Packet* BasePacket;
DWORD bytesAcked;
DWORD packetsReceived;

VOID SenderWorker::RunWorker(LPVOID p)
{
	Properties* properties = (Properties*)p;
	Packet* packet;
	LONG timeout;
	char* responseBuffer = new char[MAX_PKT_SIZE];
	HANDLE events[] = { properties->socketReceiveReady, properties->full, properties->eventQuit };

	INT kernelBuffer = 20e6;
	if (setsockopt(properties->sock, SOL_SOCKET, SO_RCVBUF, (CHAR*) &kernelBuffer, sizeof(INT)) == SOCKET_ERROR)
	{
		if (properties->workerException != NULL)
			SetEvent(properties->workerException);
		properties->errorCode = MISC_ERROR;
		return;
	}

	kernelBuffer = 20e6;
	if (setsockopt(properties->sock, SOL_SOCKET, SO_SNDBUF, (CHAR*) &kernelBuffer, sizeof(INT)) == SOCKET_ERROR)
	{
		if (properties->workerException != NULL)
			SetEvent(properties->workerException);
		properties->errorCode = MISC_ERROR;
		return;
	}
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	timerExpire = clock() + CLOCKS_PER_SEC * (properties->RTO / 1000.0);

	while (TRUE)
	{
		//printf("\n\n#### Starting new round ####\n\n");
		if (properties->numPendingPackets > 0)
		{
			packet = properties->pendingPackets + (properties->senderBase % properties->windowSize);
			//printf("\ntimer expires at clock %d, current clock %d, clocks per second: %d\n", timerExpire, clock(), CLOCKS_PER_SEC);
			timeout = ((timerExpire - clock()) / (FLOAT) CLOCKS_PER_SEC) * 1000.0;
		}
		else
		{
			timeout = INFINITE;
		}
		//printf("TIME-DEBUG: %d pending packets, senderBase: %d, RTO %ld ms\n", properties->numPendingPackets, properties->senderBase, (LONG)properties->RTO);
		//printf("TIME-DEBUG: timeout %d ms\n", timeout);

		switch (WaitForMultipleObjects(3, events, FALSE, timeout))
		{
			case WAIT_TIMEOUT:
				//printf("\nEvent 3) Sender base timed out\n");
				properties->timeoutPackets++;
				RetransmitBase(properties);
				break;
			case 0: // ACK is backed up at socket
				//printf("\nEvent 1) Socket ready\n");
				ReceiveACK(responseBuffer, properties); //TODO: Notify Sender if result != STATUS_OK (IN ALL CASES)
				break;
			case 1: // Packet has been produced and is ready to send
				packet = properties->pendingPackets + (properties->nextToSend % properties->windowSize);
				//printf("\nEvent 2) Packet produced, seq num %d, time left before RTX %d ms\nSending...\n", packet->seq, timeout);
				if (properties->nextToSend == properties->senderBase)
					resetTimer = true;
				result = sendto(properties->sock, packet->buf, packet->size, NULL, 
							   (struct sockaddr*) & properties->server, sizeof(properties->server));
				if (result == SOCKET_ERROR)
				{
					if (properties->workerException != NULL)
						SetEvent(properties->workerException);
					properties->errorCode = FAILED_SEND;
					return;
				}
				packet->TXTime = clock();
				if(packet->type != 0)
					properties->nextToSend++;
				break;
			case 2:
				return;
			default: 
				if (properties->workerException != NULL)
					SetEvent(properties->workerException);
				properties->errorCode = MISC_ERROR;
				return;
		}

		if (resetTimer)
		{
			//printf("Resetting senderBase timer...\n");
			timerExpire = clock() + CLOCKS_PER_SEC * (properties->RTO / 1000.0);
			resetTimer = false;
		}
	}

	delete[] responseBuffer;
}

VOID SenderWorker::RetransmitBase(Properties* properties)
{
	Packet* packet = properties->pendingPackets + (properties->senderBase % properties->windowSize);
	//printf("\tseq num: %d, times transmitted: %d\n", packet->seq, packet->numRTX);

	if (packet->numRTX >= MAX_RTX_ATTEMPTS)
	{
		if (properties->workerException != NULL)
			SetEvent(properties->workerException);
		properties->errorCode = TIMEOUT;
		return;
	}

	// Send next packet
	result = sendto(properties->sock, packet->buf, packet->size, NULL,
		(struct sockaddr*) &properties->server, sizeof(properties->server));

	packet->numRTX++;
	resetTimer = true;
	packet->TXTime = clock();
}

VOID SenderWorker::ReceiveACK(char* responseBuffer, Properties* properties)
{
	// Attempt to get response from server
	if (recvfrom(properties->sock, responseBuffer, MAX_PKT_SIZE, NULL, NULL, NULL) == SOCKET_ERROR)
	{
		printf("[%2.3f] <-- ", properties->totalTimer.ElapsedMilliseconds() / 1000.0);
		printf("failed recvfrom with %d\n", WSAGetLastError());
		if (properties->workerException != NULL)
			SetEvent(properties->workerException);
		properties->errorCode = FAILED_RECV;
		return;
	}
	
	ACKHeader = *(ReceiverHeader*)responseBuffer;
	if (ACKHeader.ackSeq < properties->senderBase)
	{
		//printf("ACK for packet outside window (ACK %d)\n", ACKHeader.ackSeq);
		return;
	} 

	//printf("RECV-DEBUG: Received ACK w/ value %d\n", ACKHeader.ackSeq);
	if (ACKHeader.flags.SYN || ACKHeader.flags.FIN)
	{
		AckPacket = properties->pendingPackets + (ACKHeader.ackSeq % properties->windowSize);
		//printf("RECV-DEBUG: SYN or FIN ACK detected\n");
		if (ACKHeader.flags.SYN)
		{
			properties->estRTT = 1000 * ((clock() - AckPacket->TXTime) / (FLOAT) CLOCKS_PER_SEC);
			properties->devRTT = 0;
			properties->RTO = properties->estRTT + 4 * max(properties->devRTT, 10);
			//printf("RECV-DEBUG: Setting RTO to %d ms (%d elapsed, est. RTT %.3fs est. DEV %.3fs)\n", properties->RTO, elapsedTime, properties->estRTT / 1000.0, properties->devRTT / 1000.0);
			
			InterlockedDecrement((volatile LONG*)&properties->numPendingPackets);
			lastReleased = min(properties->windowSize, ACKHeader.recvWnd);
			ReleaseSemaphore(properties->empty, lastReleased, NULL);
		} 
		else
		{
			printf("[%2.3f] <-- ", properties->totalTimer.ElapsedMilliseconds() / 1000.0);
			printf("FIN-ACK %d window 0x%X\n", ACKHeader.ackSeq, ACKHeader.recvWnd);
		}
		resetTimer = true;
		if (properties->ControlAckReceived != NULL)
			SetEvent(properties->ControlAckReceived);
		return;
	}
	else
	{
		if (ACKHeader.ackSeq > properties->senderBase)
		{
			AckPacket = properties->pendingPackets + ((ACKHeader.ackSeq - 1) % properties->windowSize);
			elapsedTime = 1000 * ((clock() - AckPacket->TXTime) / (FLOAT)CLOCKS_PER_SEC);

			packetsReceived = ACKHeader.ackSeq - properties->senderBase;
			bytesAcked = (AckPacket->size - sizeof(SenderDataHeader)) + (packetsReceived - 1) * (MAX_PKT_SIZE - sizeof(SenderDataHeader));

			properties->bytesAcked += bytesAcked;
			InterlockedAdd((volatile LONG*)&properties->goodput, bytesAcked);

			BasePacket = properties->pendingPackets + (properties->senderBase % properties->windowSize);
			if (BasePacket->numRTX == 1)
			{
				properties->estRTT = .875 * properties->estRTT + .125 * elapsedTime;
				properties->devRTT = .75 * properties->devRTT + .25 * abs(elapsedTime - properties->estRTT);
				properties->RTO = properties->estRTT + 4 * max(properties->devRTT, 10);
				//printf("RECV-DEBUG: Setting RTO to %d ms (%d elapsed, est. RTT %.3fs est. DEV %.3fs)\n", properties->RTO, elapsedTime, properties->estRTT / 1000.0, properties->devRTT / 1000.0);
			}
			//printf("RECV-DEBUG: Releasing %d slots (Receiver window %d)\n", ACKHeader.ackSeq - properties->senderBase, ACKHeader.recvWnd);
			InterlockedExchangeSubtract((volatile DWORD*)&properties->numPendingPackets, packetsReceived);

			resetTimer = true;
			properties->senderBase = ACKHeader.ackSeq;

			INT effectiveWindow = min(properties->windowSize, ACKHeader.recvWnd);
			//printf("Receiver Window Size: %d, effective Window Size: %d\n", ACKHeader.recvWnd, effectiveWindow);
			INT newReleased = properties->senderBase + effectiveWindow - lastReleased;
			//printf("Releasing: %d\n", newReleased);
			ReleaseSemaphore(properties->empty, newReleased, NULL);
			lastReleased += newReleased;
		}
	
		// Check for fast retransmission
		if (numDuplicateACKS == 0)
		{
			duplicateACKVal = ACKHeader.ackSeq;
			numDuplicateACKS++;
		}
		else if (ACKHeader.ackSeq == duplicateACKVal)
		{
			numDuplicateACKS++;
			if (numDuplicateACKS == FAST_RTX_NUM)
			{
				//printf("RECV-DEBUG: Fast retransmit signalled\n");
				properties->fastRetxPackets++;
				RetransmitBase(properties);
			}
		}
		else
		{
			numDuplicateACKS = 0;
		}
	}
}