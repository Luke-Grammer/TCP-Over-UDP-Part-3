// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

//#include <windows.h>
#include <winsock2.h>

#include <iostream>
#include <chrono>

#include "Constants.h"
#include "Headers.h"
#include "Timer.h"
#include "Packet.h"
#include "Properties.h"
#include "StatsManager.h"
#include "SenderSocket.h"
#include "Checksum.h"
#include "SenderWorker.h"

#endif //PCH_H
