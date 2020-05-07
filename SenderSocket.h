// SenderSocket.h
// CSCE 463-500
// Luke Grammer
// 11/12/19

#pragma once

class SenderSocket
{
	HANDLE statsHandle, workerHandle;
	struct Properties* properties;
	BOOLEAN connected = false;
	DWORD nextSequenceNum = 0;
	
	/* GetServerInfo does a forward lookup on the destination host string if necessary
	 * and populates it's internal server information with the result. Called in Open().
	 * Returns code 0 to indicate success or 3 if the target hostname does not have an
	 * entry in DNS. */
	WORD GetServerInfo(CONST CHAR* destination, WORD port);



public:
	/* Constructor initializes WinSock and sets up a UDP socket for RDP.
	 * In addition, the server also starts a timer for the life of the
	 * SenderSocket object. calls exit() if socket creation is unsuccessful. */
	SenderSocket(Properties* p);

	/* Basic destructor for SenderSocket cleans up socket and WinSock. */
	~SenderSocket();

	/* Open() calls GetServerInfo() to populate internal server information and then creates a handshake
	 * packet and attempts to send it to the corresponding server. If un-acknowledged, it will retransmit
	 * this packet up to MAX_SYN_ATTEMPS times (by default 3). Open() will set the retransmission
	 * timeout for future communication with the server to a constant scale of the handshake RTT.
	 * Returns 0 to indicate success or a positive number to indicate failure. */
	WORD Open(CONST CHAR* destination, WORD port, DWORD senderWindow, struct LinkProperties* lp);
	
	/* Attempts to send a single packet to the connected server. This is the externally facing
	 * Send() function and therefore requires a previously successful call to Open().
	 * Returns 0 to indicate success or a positive number for failure. */
	WORD Send(CONST CHAR* message, INT messageSize, INT messageType = DATA_TYPE);

	/* Closes connection to the current server. Sends a connection termination packet and waits
     * for an acknowledgement using the RTO calculated in the call to Open(). Returns 0 to
     * indicate success or a positive number for failure.*/
	WORD Close(DOUBLE& elapsedTime);
};