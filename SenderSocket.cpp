// SenderSocket.cpp
// CSCE 463-500
// Luke Grammer
// 11/21/19

#include "pch.h"

/* Constructor initializes WinSock and sets up a UDP socket for RDP. 
 * In addition, the server also starts a timer for the life of the 
 * SenderSocket object. calls exit() if socket creation is unsuccessful. */
SenderSocket::SenderSocket(Properties* p)
{
	properties = p;

	struct WSAData wsaData;
	WORD wVerRequested;

	//initialize WinSock
	wVerRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVerRequested, &wsaData) != 0) {
		printf("\tWSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	// open a UDP socket
	properties->sock = socket(AF_INET, SOCK_DGRAM, NULL);
	if (properties->sock == INVALID_SOCKET)
	{
		printf("\tsocket() generated error %d\n", WSAGetLastError());
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	// Bind socket to local machine
	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = NULL;

	if (bind(properties->sock, (struct sockaddr*) &local, sizeof(local)) == SOCKET_ERROR)
	{
		printf("bind() generated error %d\n", WSAGetLastError());
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	// Set up address for local DNS server
	memset(&properties->server, 0, sizeof(properties->server));
	properties->server.sin_family = AF_INET;

	properties->totalTimer = Timer();

}

/* Basic destructor for SenderSocket cleans up socket and WinSock. */
SenderSocket::~SenderSocket()
{
	//cout << "DEBUG: Stats handle closed, cleaning up\n";
	closesocket(properties->sock);
	WSACleanup();
}

/* GetServerInfo does a forward lookup on the destination host string if necessary
 * and populates it's internal server information with the result. Called in Open().
 * Returns code 0 to indicate success or 3 if the target hostname does not have an 
 * entry in DNS. */
WORD SenderSocket::GetServerInfo(CONST CHAR* destination, WORD port)
{
	struct hostent* remote;

	// Disable inet_addr deprecation warning
	#pragma warning(disable:4996) 
	DWORD destinationIP = inet_addr(destination);

	// host is a valid IP, do not do a DNS lookup
	if (destinationIP != INADDR_NONE)
		properties->server.sin_addr.S_un.S_addr = destinationIP;
	else
	{
		if ((remote = gethostbyname(destination)) == NULL)
		{
			// failure in gethostbyname
			printf("[%2.3f] --> ", properties->totalTimer.ElapsedMilliseconds() / 1000.0);
			printf("target %s is invalid\n", destination);
			return INVALID_NAME;
		}
		// take the first IP address and copy into sin_addr
		else
		{
			destinationIP = *(u_long*)remote->h_addr;
			memcpy((char*) & (properties->server.sin_addr), remote->h_addr, remote->h_length);
		}
	}
	properties->server.sin_port = htons(port);
	properties->serverAddr.s_addr = destinationIP;

	return STATUS_OK;
}

/* Open() calls GetServerInfo() to populate internal server information and then creates a handshake
 * packet and attempts to send it to the corresponding server. If un-acknowledged, it will retransmit
 * this packet up to MAX_SYN_ATTEMPS times (by default 3). Open() will set the retransmission
 * timeout for future communication with the server to a constant scale of the handshake RTT.
 * Returns 0 to indicate success or a positive number to indicate failure. */
WORD SenderSocket::Open(CONST CHAR* destination, WORD port, DWORD senderWindow, struct LinkProperties* lp)
{
	INT result = -1;
	properties->windowSize = senderWindow;
	properties->RTO = max(1000, (INT) (2 * (lp->RTT * 1000)));
	
	if (connected)
		return ALREADY_CONNECTED;

	if ((result = GetServerInfo(destination, port)) != STATUS_OK)
		return result;

	// create handshake packet
	SenderSynHeader handshake;
	handshake.sdh.flags.SYN = 1;
	handshake.sdh.seq = properties->senderBase;
	handshake.lp = *lp;
	handshake.lp.bufferSize = senderWindow + MAX_RTX_ATTEMPTS;

	CHAR* buf = new CHAR[sizeof(handshake)];
	memcpy(buf, &handshake, sizeof(handshake));

	result = Send(buf, sizeof(SenderSynHeader), SYN_TYPE);

	// Create Worker thread
	if ((result = WSAEventSelect(properties->sock, properties->socketReceiveReady, FD_READ)) != 0)
	{
		printf("Could not associate socket with event (status code %d) exiting...\n", result);
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	workerHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SenderWorker::RunWorker, properties, 0, NULL);
	if (workerHandle == NULL)
	{
		printf("Could not create worker thread! exiting...\n");
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	HANDLE events[] = { properties->workerException, properties->ControlAckReceived };
	switch (WaitForMultipleObjects(2, events, FALSE, INFINITE))
	{
	case 0:
		return properties->errorCode;
	case 1:
		connected = true;
		break;
	default:
		return MISC_ERROR;
	}

	// Create Stats thread
	statsHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)StatsManager::PrintStats, properties, 0, NULL);
	if (statsHandle == NULL)
	{
		printf("Could not create stats thread! exiting...\n");
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	// all attempts timed out, return
	delete[] buf;
	return STATUS_OK;
}

/* Attempts to send a single packet to the connected server. This is the externally facing 
 * Send() function and therefore requires a previously successful call to Open(). 
 * Returns 0 to indicate success or a positive number for failure. */
WORD SenderSocket::Send(CONST CHAR* message, INT messageSize, INT messageType)
{
	// If client is not connected, return
	if (!connected && messageType != SYN_TYPE)
		return NOT_CONNECTED;

	HANDLE events[] = { properties->workerException, properties->empty };

	// Wait for quit event or empty buffer slots
	switch (WaitForMultipleObjects(2, events, FALSE, INFINITE))
	{
	case 0:
		return properties->errorCode;
	case 1:
		break;
	default:
		return MISC_ERROR;
	}

	// Get next sequence number and corresponding buffer slot
	DWORD slot = nextSequenceNum % properties->windowSize;

	Packet* packet = properties->pendingPackets + slot;

	// Free old packet contents if necessary
	if (packet->buf != NULL)
		delete[] packet->buf;
	
	packet->seq      = nextSequenceNum;
	packet->type     = messageType;
	packet->size     = messageSize;
	packet->TXTime   = clock();
	packet->numRTX   = 1;

	if (messageType == DATA_TYPE)
	{
		
		//printf("SEND-DEBUG: Sending DATA Packet %d\n", seqNum);
		SenderDataHeader header;
		header.seq = nextSequenceNum;
		
		packet->size = messageSize + sizeof(SenderDataHeader);
		packet->buf = new char[sizeof(SenderDataHeader) + messageSize];
		memcpy(packet->buf, &header, sizeof(SenderDataHeader));
		memcpy(packet->buf + sizeof(SenderDataHeader), message, messageSize);
		nextSequenceNum++;
	} 
	else if (messageType == SYN_TYPE)
	{
		//printf("SEND-DEBUG: Sending ACK Packet %d\n", seqNum);
		packet->buf = new char[sizeof(SenderSynHeader)];
		memcpy(packet->buf, message, sizeof(SenderSynHeader));
	}
	else if (messageType == FIN_TYPE)
	{
		//printf("SEND-DEBUG: Sending FIN Packet %d\n", seqNum);
		SenderDataHeader header;
		header.flags.FIN = 1;
		header.seq = nextSequenceNum;
		packet->buf = new char[sizeof(SenderDataHeader)];
		memcpy(packet->buf, &header, sizeof(SenderDataHeader));
	}
	else
		return INVALID_ARGUMENTS;

	InterlockedIncrement((volatile LONG*)&properties->numPendingPackets);
	ReleaseSemaphore(properties->full, 1, NULL);

	return STATUS_OK;
}

/* Closes connection to the current server. Sends a connection termination packet and waits 
 * for an acknowledgement using the RTO calculated in the call to Open(). Returns 0 to 
 * indicate success or a positive number for failure.*/
WORD SenderSocket::Close(DOUBLE &elapsedTime)
{
	if (!connected)
		return NOT_CONNECTED;

	// create connection termination packet
	Send(NULL, sizeof(SenderDataHeader), FIN_TYPE);

	HANDLE events[] = { properties->workerException, properties->ControlAckReceived };
	switch (WaitForMultipleObjects(2, events, FALSE, INFINITE))
	{
	case 0:
		return properties->errorCode;
	case 1:
		connected = false;
		break;
	default:
		return MISC_ERROR;
	}

	if (properties->eventQuit != NULL)
		SetEvent(properties->eventQuit);

	WaitForSingleObject(statsHandle, INFINITE);
	CloseHandle(statsHandle);

	WaitForSingleObject(workerHandle, INFINITE);
	CloseHandle(workerHandle);

	// all attempts timed out, return
	return STATUS_OK;
}