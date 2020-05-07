// Timer.h
// CSCE 463-500
// Luke Grammer
// 11/21/19

#pragma once

using namespace std::chrono;

class Timer
{
	time_point<high_resolution_clock> start  = high_resolution_clock::now();
	time_point<high_resolution_clock> end    = high_resolution_clock::now();
	BOOL stopCalled = FALSE;

public:
	VOID Start()
	{ 
		start = high_resolution_clock::now(); 
		stopCalled = false;
	}

	VOID Stop()               
	{ 
		end = high_resolution_clock::now();  
		stopCalled = true;
	}

	LONGLONG ElapsedSeconds()
	{ 
		if (stopCalled)
			return duration_cast<std::chrono::seconds>(end - start).count();     
		else
			return duration_cast<std::chrono::seconds>(high_resolution_clock::now() - start).count();

	}

	LONGLONG ElapsedMilliseconds() 
	{ 
		if (stopCalled)
			return duration_cast<std::chrono::milliseconds>(end - start).count();
		else
			return duration_cast<std::chrono::milliseconds>(high_resolution_clock::now() - start).count();
	}

	LONGLONG ElapsedMicroseconds()
	{
		if (stopCalled)
			return duration_cast<std::chrono::microseconds>(end - start).count();
		else
			return duration_cast<std::chrono::microseconds>(high_resolution_clock::now() - start).count();
	}

	LONGLONG ElapsedNanoseconds()  
	{ 
		if (stopCalled)
			return duration_cast<std::chrono::nanoseconds>(end - start).count();
		else
			return duration_cast<std::chrono::nanoseconds>(high_resolution_clock::now() - start).count();
	}
};