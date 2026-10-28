#pragma once
#include "framework.h"

extern HANDLE g_redrawEvent;

class Worker final {
public:
	Worker(POINT click_point);
	~Worker();

	static std::shared_ptr<Worker> CreateWorker(POINT click_point);

	DWORD ThreadCounter();
	void ExitThread();

	DWORD GetThreadID() const;
	POINT GetClickPoint() const;

	uint32_t GetCounter() const;

	bool IsRunning() const;

private:

	static DWORD WINAPI StartThread(LPVOID lpParam);

	HANDLE m_thread;
	DWORD m_threadId;

	POINT m_clickPoint;
	uint32_t m_counter; // Read-only
	std::chrono::steady_clock::time_point m_startTime = std::chrono::steady_clock::now();

	std::atomic<bool> m_exit = false;
};