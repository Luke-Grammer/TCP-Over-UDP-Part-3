// Packet.h
// CSCE 463-500
// Luke Grammer
// 11/21/19

#pragma once

#pragma pack(push, 1)
struct Packet 
{
	int seq = 0; // for easy access in worker thread
	int type = DATA_TYPE; // SYN, FIN, data
	int size = 0; // for retx in worker thread
	int numRTX = 0;
	//Timer RTXTimer = Timer(); // transmission start time
	clock_t TXTime;
	char* buf = NULL; // actual packet with header 
};
#pragma pack(pop)