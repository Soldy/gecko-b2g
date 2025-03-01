/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StaticPrefs_page_load.h"
#include "mozilla/Unused.h"
#include "mozilla/ipc/IdleSchedulerParent.h"
#include "nsSystemInfo.h"
#include "nsThreadUtils.h"
#include "nsITimer.h"
#include "nsIThread.h"

namespace mozilla {
namespace ipc {

base::SharedMemory* IdleSchedulerParent::sActiveChildCounter = nullptr;
std::bitset<NS_IDLE_SCHEDULER_COUNTER_ARRAY_LENGHT>
    IdleSchedulerParent::sInUseChildCounters;
LinkedList<IdleSchedulerParent> IdleSchedulerParent::sWaitingForIdle;
Atomic<int32_t> IdleSchedulerParent::sMaxConcurrentIdleTasksInChildProcesses(
    -1);
uint32_t IdleSchedulerParent::sChildProcessesRunningPrioritizedOperation = 0;
uint32_t IdleSchedulerParent::sChildProcessesAlive = 0;
nsITimer* IdleSchedulerParent::sStarvationPreventer = nullptr;

IdleSchedulerParent::IdleSchedulerParent() {
  sChildProcessesAlive++;

  if (sMaxConcurrentIdleTasksInChildProcesses == -1) {
    // nsISystemInfo can be initialized only on the main thread.
    // While waiting for the real logical core count behave as if there was just
    // one core.
    sMaxConcurrentIdleTasksInChildProcesses = 1;
    nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
    nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction("cpucount getter", [thread]() {
          // Always pretend that there is at least one core for child processes.
          // If there are multiple logical cores, reserve one for the parent
          // process and for the non-main threads.
          ProcessInfo processInfo = {};
          if (NS_SUCCEEDED(CollectProcessInfo(processInfo)) &&
              processInfo.cpuCount > 1) {
            // On one and two processor (or hardware thread) systems this will
            // allow one concurrent idle task.
            sMaxConcurrentIdleTasksInChildProcesses =
                std::max(processInfo.cpuCount - 1, 1);
            // We have a new cpu count, reschedule idle scheduler.
            nsCOMPtr<nsIRunnable> runnable =
                NS_NewRunnableFunction("IdleSchedulerParent::Schedule", []() {
                  if (sActiveChildCounter && sActiveChildCounter->memory()) {
                    static_cast<Atomic<int32_t>*>(sActiveChildCounter->memory())
                        [NS_IDLE_SCHEDULER_INDEX_OF_CPU_COUNTER] =
                            static_cast<int32_t>(
                                sMaxConcurrentIdleTasksInChildProcesses);
                  }
                  IdleSchedulerParent::Schedule(nullptr);
                });
            thread->Dispatch(runnable, NS_DISPATCH_NORMAL);
          }
        });
    NS_DispatchBackgroundTask(runnable.forget(), NS_DISPATCH_EVENT_MAY_BLOCK);
  }
}

IdleSchedulerParent::~IdleSchedulerParent() {
  // We can't know if an active process just crashed, so we just always expect
  // that is the case.
  if (mChildId) {
    sInUseChildCounters[mChildId] = false;
    if (sActiveChildCounter && sActiveChildCounter->memory() &&
        static_cast<Atomic<int32_t>*>(
            sActiveChildCounter->memory())[mChildId]) {
      --static_cast<Atomic<int32_t>*>(
          sActiveChildCounter
              ->memory())[NS_IDLE_SCHEDULER_INDEX_OF_ACTIVITY_COUNTER];
      static_cast<Atomic<int32_t>*>(sActiveChildCounter->memory())[mChildId] =
          0;
    }
  }

  if (mRunningPrioritizedOperation) {
    --sChildProcessesRunningPrioritizedOperation;
  }

  if (isInList()) {
    remove();
  }

  MOZ_ASSERT(sChildProcessesAlive > 0);
  sChildProcessesAlive--;
  if (sChildProcessesAlive == 0) {
    MOZ_ASSERT(sWaitingForIdle.isEmpty());
    delete sActiveChildCounter;
    sActiveChildCounter = nullptr;

    if (sStarvationPreventer) {
      sStarvationPreventer->Cancel();
      NS_RELEASE(sStarvationPreventer);
    }
  }

  Schedule(nullptr);
}

IPCResult IdleSchedulerParent::RecvInitForIdleUse(
    InitForIdleUseResolver&& aResolve) {
  // This must already be non-zero, if it is zero then the cleanup code for the
  // shared memory (initialised below) will never run.  The invariant is that if
  // the shared memory is initialsed, then this is non-zero.
  MOZ_ASSERT(sChildProcessesAlive > 0);

  MOZ_ASSERT(IsNotDoingIdleTask());

  // Create a shared memory object which is shared across all the relevant
  // processes.
  if (!sActiveChildCounter) {
    sActiveChildCounter = new base::SharedMemory();
    size_t shmemSize = NS_IDLE_SCHEDULER_COUNTER_ARRAY_LENGHT * sizeof(int32_t);
    if (sActiveChildCounter->Create(shmemSize) &&
        sActiveChildCounter->Map(shmemSize)) {
      memset(sActiveChildCounter->memory(), 0, shmemSize);
      sInUseChildCounters[NS_IDLE_SCHEDULER_INDEX_OF_ACTIVITY_COUNTER] = true;
      sInUseChildCounters[NS_IDLE_SCHEDULER_INDEX_OF_CPU_COUNTER] = true;
      static_cast<Atomic<int32_t>*>(
          sActiveChildCounter
              ->memory())[NS_IDLE_SCHEDULER_INDEX_OF_CPU_COUNTER] =
          static_cast<int32_t>(sMaxConcurrentIdleTasksInChildProcesses);
    } else {
      delete sActiveChildCounter;
      sActiveChildCounter = nullptr;
    }
  }
  Maybe<SharedMemoryHandle> activeCounter;
  SharedMemoryHandle handle;
  if (sActiveChildCounter &&
      sActiveChildCounter->ShareToProcess(OtherPid(), &handle)) {
    activeCounter.emplace(handle);
  }

  uint32_t unusedId = 0;
  for (uint32_t i = 0; i < NS_IDLE_SCHEDULER_COUNTER_ARRAY_LENGHT; ++i) {
    if (!sInUseChildCounters[i]) {
      sInUseChildCounters[i] = true;
      unusedId = i;
      break;
    }
  }

  // If there wasn't an empty item, we'll fallback to 0.
  mChildId = unusedId;

  aResolve(Tuple<const mozilla::Maybe<SharedMemoryHandle>&, const uint32_t&>(
      activeCounter, mChildId));
  return IPC_OK();
}

IPCResult IdleSchedulerParent::RecvRequestIdleTime(uint64_t aId,
                                                   TimeDuration aBudget) {
  MOZ_ASSERT(aBudget);
  MOZ_ASSERT(IsNotDoingIdleTask());

  mCurrentRequestId = aId;
  mRequestedIdleBudget = aBudget;

  sWaitingForIdle.insertBack(this);

  Schedule(this);
  return IPC_OK();
}

IPCResult IdleSchedulerParent::RecvIdleTimeUsed(uint64_t aId) {
  // The client can either signal that they've used the idle time or they're
  // canceling the request.  We cannot use a seperate cancel message because it
  // could arrive after the parent has granted the request.
  MOZ_ASSERT(IsWaitingForIdle() || IsDoingIdleTask());

  // The parent process will always know the ID of the current request (since
  // the IPC channel is reliable).  The IDs are provided so that the client can
  // check them (it's possible for the client to race ahead of the server).
  MOZ_ASSERT(mCurrentRequestId == aId);

  if (IsWaitingForIdle()) {
    remove();
  }
  mRequestedIdleBudget = TimeDuration();
  Schedule(nullptr);
  return IPC_OK();
}

IPCResult IdleSchedulerParent::RecvSchedule() {
  Schedule(nullptr);
  return IPC_OK();
}

IPCResult IdleSchedulerParent::RecvRunningPrioritizedOperation() {
  ++mRunningPrioritizedOperation;
  if (mRunningPrioritizedOperation == 1) {
    ++sChildProcessesRunningPrioritizedOperation;
  }
  return IPC_OK();
}

IPCResult IdleSchedulerParent::RecvPrioritizedOperationDone() {
  MOZ_ASSERT(mRunningPrioritizedOperation);

  --mRunningPrioritizedOperation;
  if (mRunningPrioritizedOperation == 0) {
    --sChildProcessesRunningPrioritizedOperation;
    Schedule(nullptr);
  }
  return IPC_OK();
}

int32_t IdleSchedulerParent::ActiveCount() {
  if (sActiveChildCounter) {
    return (static_cast<Atomic<int32_t>*>(
        sActiveChildCounter
            ->memory())[NS_IDLE_SCHEDULER_INDEX_OF_ACTIVITY_COUNTER]);
  }
  return 0;
}

bool IdleSchedulerParent::HasSpareCycles(int32_t aActiveCount) {
  // We can run a new task if we have a spare core.  If we're running a
  // prioritised operation we halve the number of regular spare cores.
  //
  // sMaxConcurrentIdleTasksInChildProcesses will always be >0 so on 1 and 2
  // core systems this will allow 1 idle tasks (0 if running a prioritized
  // operation).
  MOZ_ASSERT(sMaxConcurrentIdleTasksInChildProcesses > 0);
  return sChildProcessesRunningPrioritizedOperation
             ? sMaxConcurrentIdleTasksInChildProcesses / 2 > aActiveCount
             : sMaxConcurrentIdleTasksInChildProcesses > aActiveCount;
}

void IdleSchedulerParent::SendIdleTime() {
  // We would assert that IsWaiting() except after removing the task from it's
  // list this will return false.  Instead check IsDoingIdleTask()
  MOZ_ASSERT(IsDoingIdleTask());
  Unused << SendIdleTime(mCurrentRequestId, mRequestedIdleBudget);
}

void IdleSchedulerParent::Schedule(IdleSchedulerParent* aRequester) {
  // Tasks won't update the active count until after they receive their message
  // and start to run, so make a copy of it here and increment it for every task
  // we schedule. It will become an estimate of how many tasks will be active
  // shortly.
  int32_t activeCount = ActiveCount();

  if (aRequester && aRequester->mRunningPrioritizedOperation) {
    // If the requester is prioritized, just let it run itself.
    if (aRequester->isInList()) {
      aRequester->remove();
    }
    aRequester->SendIdleTime();
    activeCount++;
  }

  while (!sWaitingForIdle.isEmpty() && HasSpareCycles(activeCount)) {
    // We can run an idle task.
    RefPtr<IdleSchedulerParent> idleRequester = sWaitingForIdle.popFirst();
    idleRequester->SendIdleTime();
    activeCount++;
  }

  if (!sWaitingForIdle.isEmpty()) {
    EnsureStarvationTimer();
  }
}

void IdleSchedulerParent::EnsureStarvationTimer() {
  // Even though idle runnables aren't really guaranteed to get run ever (which
  // is why most of them have the timer fallback), try to not let any child
  // process' idle handling to starve forever in case other processes are busy
  if (!sStarvationPreventer) {
    // Reuse StaticPrefs::page_load_deprioritization_period(), since that
    // is used on child side when deciding the minimum idle period.
    NS_NewTimerWithFuncCallback(
        &sStarvationPreventer, StarvationCallback, nullptr,
        StaticPrefs::page_load_deprioritization_period(),
        nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY, "StarvationCallback");
  }
}

void IdleSchedulerParent::StarvationCallback(nsITimer* aTimer, void* aData) {
  if (!sWaitingForIdle.isEmpty()) {
    RefPtr<IdleSchedulerParent> first = sWaitingForIdle.getFirst();
    // Treat the first process waiting for idle time as running prioritized
    // operation so that it gets run.
    ++first->mRunningPrioritizedOperation;
    ++sChildProcessesRunningPrioritizedOperation;
    Schedule(first);
    --first->mRunningPrioritizedOperation;
    --sChildProcessesRunningPrioritizedOperation;
  }
  NS_RELEASE(sStarvationPreventer);
}

}  // namespace ipc
}  // namespace mozilla
