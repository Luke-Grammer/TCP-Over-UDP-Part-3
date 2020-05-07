// Constants.h
// CSCE 463-500
// Luke Grammer
// 11/21/19

#pragma once

#define FORWARD_PATH      0
#define RETURN_PATH       1
#define SYN_TYPE          0
#define FIN_TYPE          1
#define DATA_TYPE         2
#define FAST_RTX_NUM      3
#define MAX_RTX_ATTEMPTS  50
#define STATS_INTERVAL    2
#define MAGIC_PROTOCOL    0x8311AA
#define MAGIC_PORT        22345
#define MAX_PKT_SIZE      (1500 - 28)

// possible status codes from ss.Open(), ss.Send(), and ss.Close()
#define STATUS_OK         0 // no error
#define ALREADY_CONNECTED 1 // second call to ss.Open() without closing connection
#define NOT_CONNECTED     2 // call to ss.Send()/ss.Close() without ss.Open()
#define INVALID_NAME      3 // ss.Open() with target host that has no DNS entry
#define FAILED_SEND       4 // sendto() failed in kernel 
#define TIMEOUT           5 // timeout after all retx attempts are exhausted
#define FAILED_RECV       6 // recvfrom() failed in kernel
#define INVALID_ARGUMENTS 7 // incorrect number or out of range arguments
#define MISC_ERROR        8           