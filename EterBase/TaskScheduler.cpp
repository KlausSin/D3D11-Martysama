#include "TaskScheduler.h"
#include <TaskScheduler.h>

CTaskScheduler& CTaskScheduler::Instance()
{
	static CTaskScheduler s;
	return s;
}

CTaskScheduler::~CTaskScheduler()
{
	Shutdown();
}

void CTaskScheduler::Initialize(uint32_t numThreads)
{
	if (m_scheduler)
		return;

	m_scheduler = new enki::TaskScheduler();

	enki::TaskSchedulerConfig config;
	if (numThreads > 0)
		config.numTaskThreadsToCreate = numThreads;

	m_scheduler->Initialize(config);
	m_numThreads = m_scheduler->GetNumTaskThreads();
}

void CTaskScheduler::Shutdown()
{
	if (!m_scheduler)
		return;

	m_scheduler->WaitforAllAndShutdown();
	delete m_scheduler;
	m_scheduler = nullptr;
	m_numThreads = 0;
}

void CTaskScheduler::AddTaskSetToPipe(enki::ITaskSet* pTask)
{
	if (m_scheduler)
		m_scheduler->AddTaskSetToPipe(pTask);
}

void CTaskScheduler::WaitforTask(enki::ITaskSet* pTask)
{
	if (m_scheduler)
		m_scheduler->WaitforTask(pTask);
}

void CTaskScheduler::WaitforAll()
{
	if (m_scheduler)
		m_scheduler->WaitforAll();
}
