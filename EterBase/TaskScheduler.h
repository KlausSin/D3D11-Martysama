#pragma once

#include <cstdint>

namespace enki {
	class TaskScheduler;
	class ITaskSet;
	class IPinnedTask;
}

class CTaskScheduler
{
public:
	static CTaskScheduler& Instance();

	void Initialize(uint32_t numThreads = 0);
	void Shutdown();

	enki::TaskScheduler* Get() const { return m_scheduler; }
	uint32_t GetNumThreads() const { return m_numThreads; }
	bool IsInitialized() const { return m_scheduler != nullptr; }

	void AddTaskSetToPipe(enki::ITaskSet* pTask);
	void WaitforTask(enki::ITaskSet* pTask);
	void WaitforAll();

private:
	CTaskScheduler() = default;
	~CTaskScheduler();
	CTaskScheduler(const CTaskScheduler&) = delete;
	CTaskScheduler& operator=(const CTaskScheduler&) = delete;

	enki::TaskScheduler* m_scheduler = nullptr;
	uint32_t m_numThreads = 0;
};
