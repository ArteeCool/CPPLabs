#include "Worker.h"

Worker::Worker(POINT click_point) : m_clickPoint(click_point), m_counter(0) {

}

Worker::~Worker() {
	ExitThread();
}

std::shared_ptr<Worker> Worker::CreateWorker(POINT click_point) {
	std::shared_ptr<Worker> worker = std::make_shared<Worker>(click_point);
	worker->m_thread = CreateThread(nullptr, 0, &Worker::StartThread, worker.get(), 0, nullptr);
	worker->m_threadId = GetThreadId(worker->m_thread);
	return worker;
}

DWORD WINAPI Worker::StartThread(LPVOID lpParam) {
	Worker* pworker = reinterpret_cast<Worker*>(lpParam);
	return pworker->ThreadCounter();
}

DWORD Worker::ThreadCounter() {
	while (!m_exit.load(std::memory_order_acquire)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		auto currentTime = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::seconds>(currentTime - m_startTime);
		
		uint32_t newCounter = static_cast<uint32_t>(duration.count());

		if (newCounter != m_counter) {
			m_counter = newCounter;
			SetEvent(g_redrawEvent);
		}
	}
	return 0;
}

void Worker::ExitThread() {
	if (m_thread) {
		m_exit.store(true, std::memory_order_release);
	
		WaitForSingleObject(m_thread, INFINITE);

		CloseHandle(m_thread);
		m_thread = nullptr;
		m_threadId = 0;
	}
}

DWORD Worker::GetThreadID() const {
	return m_threadId;
}

POINT Worker::GetClickPoint() const {
	return m_clickPoint;
}

uint32_t Worker::GetCounter() const {
	return m_counter;
}

bool Worker::IsRunning() const {
	return not m_exit.load();
}