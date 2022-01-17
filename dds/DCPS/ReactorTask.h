/*
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_REACTORTASK_H
#define OPENDDS_DCPS_REACTORTASK_H

#include "dcps_export.h"
#include "RcObject.h"
#include "TimeTypes.h"
#include "ReactorInterceptor.h"
#include "SafetyProfileStreams.h"
#include "ConditionVariable.h"
#include "ThreadStatusManager.h"

#include <ace/Task.h>
#include <ace/Synch_Traits.h>
#include <ace/Timer_Heap_T.h>
#include <ace/Event_Handler_Handle_Timeout_Upcall.h>

ACE_BEGIN_VERSIONED_NAMESPACE_DECL
class ACE_Proactor;
class ACE_Reactor;
ACE_END_VERSIONED_NAMESPACE_DECL

OPENDDS_BEGIN_VERSIONED_NAMESPACE_DECL

namespace OpenDDS {
namespace DCPS {

class OpenDDS_Dcps_Export ReactorTask : public virtual ACE_Task_Base,
public virtual RcObject {

public:

  explicit ReactorTask(bool useAsyncSend);
  virtual ~ReactorTask();

public:
  int open_reactor_task(void*,
                        ThreadStatusManager* thread_status_manager = 0,
                        const String& name = "");
  virtual int open(void* ptr) {
    return open_reactor_task(ptr);
  }
  virtual int svc();
  virtual int close(u_long flags = 0);

  void stop();

  ACE_Reactor* get_reactor();
  const ACE_Reactor* get_reactor() const;

  ACE_thread_t get_reactor_owner() const;

  ACE_Proactor* get_proactor();
  const ACE_Proactor* get_proactor() const;

  void wait_for_startup()
  {
    while (state_ != STATE_RUNNING) {
      condition_.wait(*thread_status_manager_);
    }
  }

  bool is_shut_down() const { return state_ == STATE_NOT_RUNNING; }

  ReactorInterceptor_rch interceptor() const { return interceptor_; }

  OPENDDS_POOL_ALLOCATION_FWD

private:

  void cleanup();

  typedef ACE_SYNCH_MUTEX LockType;
  typedef ACE_Guard<LockType> GuardType;
  typedef ConditionVariable<LockType> ConditionVariableType;
  typedef ACE_Timer_Heap_T<
    ACE_Event_Handler*, ACE_Event_Handler_Handle_Timeout_Upcall,
    ACE_SYNCH_RECURSIVE_MUTEX, MonotonicClock> TimerQueueType;

  enum State { STATE_NOT_RUNNING, STATE_OPENING, STATE_RUNNING };

  class Interceptor : public DCPS::ReactorInterceptor {
  public:
    explicit Interceptor(DCPS::ReactorTask* task)
     : ReactorInterceptor(task->get_reactor(), task->get_reactor_owner())
     , task_(task)
     {}
    bool reactor_is_shut_down() const
    {
      return task_->is_shut_down();
    }

  private:
    DCPS::ReactorTask* const task_;
  };

  LockType      lock_;
  State         state_;
  ConditionVariableType condition_;
  ACE_Reactor*  reactor_;
  ACE_thread_t  reactor_owner_;
  ACE_Proactor* proactor_;

#if defined ACE_WIN32 && defined ACE_HAS_WIN32_OVERLAPPED_IO
#define OPENDDS_REACTOR_TASK_ASYNC
  bool use_async_send_;
#endif

  TimerQueueType* timer_queue_;

  // thread status reporting
  String name_;

  ReactorInterceptor_rch interceptor_;
  ThreadStatusManager* thread_status_manager_;
};

} // namespace DCPS
} // namespace OpenDDS

OPENDDS_END_VERSIONED_NAMESPACE_DECL

#if defined (__ACE_INLINE__)
#include "ReactorTask.inl"
#endif /* __ACE_INLINE__ */

#endif  /* OPENDDS_DCPS_REACTORTASK_H */
