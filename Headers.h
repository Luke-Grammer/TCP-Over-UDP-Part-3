// Headers.h
// CSCE 463-500
// Luke Grammer
// 11/21/19

#pragma once

#pragma pack(push, 1)
struct Flags 
{
	DWORD reserved : 5; // must be 0
	DWORD      SYN : 1;
	DWORD      ACK : 1;
	DWORD      FIN : 1;
	DWORD    magic : 24;

	Flags() { memset(this, 0, sizeof(*this)); magic = MAGIC_PROTOCOL; }
};

struct LinkProperties 
{
	// transfer parameters
	FLOAT RTT;        // propogation RTT (in sec)
	FLOAT speed;      // bottleneck bandwidth (in bits/sec)
	FLOAT pLoss[2];   // probability of loss in each direction
	DWORD bufferSize; // buffer size of emulated routers (in packets)

	LinkProperties() { memset(this, 0, sizeof(*this)); }
};

struct SenderDataHeader
{
	Flags flags;
	DWORD seq = 0;   // must begin from 0
};

struct SenderSynHeader 
{
	SenderDataHeader sdh;
	LinkProperties   lp;
};

struct ReceiverHeader 
{
	Flags flags;
	DWORD recvWnd; // reciever window for flow control (in packets)
	DWORD ackSeq;  // ack value = next expected sequence
};
#pragma pack(pop)