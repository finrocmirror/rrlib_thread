//
// You received this file as part of RRLib
// Robotics Research Library
//
// Copyright (C) Finroc GbR (finroc.org)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//----------------------------------------------------------------------
/*!\file    rrlib/thread/tThread.cpp
 *
 * \author  Max Reichardt
 *
 * \date    2012-07-05
 *
 */
//----------------------------------------------------------------------
#include "rrlib/thread/tThread.h"

//----------------------------------------------------------------------
// External includes (system with <>, local with "")
//----------------------------------------------------------------------
#include <sstream>
#include <cstring>
#include "rrlib/util/patterns/singleton.h"

//----------------------------------------------------------------------
// Internal includes with ""
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Debugging
//----------------------------------------------------------------------
#include <cassert>

//----------------------------------------------------------------------
// Namespace usage
//----------------------------------------------------------------------
using namespace rrlib::logging;

//----------------------------------------------------------------------
// Namespace declaration
//----------------------------------------------------------------------
namespace rrlib
{
namespace thread
{

//----------------------------------------------------------------------
// Forward declarations / typedefs / enums
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Const values
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Implementation
//----------------------------------------------------------------------

__thread tThread* tThread::cur_thread = NULL;


namespace internal
{
class tThreadCleanup
{
public:
  static void Cleanup(tThread* t)
  {
    tLock l(*t);
    t->state = tThread::tState::TERMINATED;
    //t->curThread.reset();
    l.Unlock();
    t->self.reset(); // possibly delete thread - important that it's last statement
  }
};

/*!
 * Class for thread (self) deletion
 */
class tThreadDeleter
{
public:
  //! for (self) deletion
  void operator()(tThread* t)
  {
    if (t->GetDeleteOnCompletion())
    {
      delete t;
    }
  }
};

static std::string GetDefaultThreadName(int64_t id)
{
  std::ostringstream oss;
  oss << "Thread-" << id;
  return oss.str();
}

template <typename T>
struct CreateCurThreadLocal
{
  static T* Create()
  {
    return new T(tThreadCleanup::Cleanup);
  }
  static void Destroy(T* object)
  {
    delete object;
  }
};

/*! Thread counter - used for generating names */
static std::atomic<int>& GetThreadCounter()
{
  static std::atomic<int> thread_counter;
  return thread_counter;
}

/*! List of threads currently known and running (== all thread objects created) */
static std::shared_ptr<internal::tVectorWithMutex<std::weak_ptr<tThread>>>& GetThreadList()
{
  static std::shared_ptr<internal::tVectorWithMutex<std::weak_ptr<tThread>>> thread_list(new internal::tVectorWithMutex<std::weak_ptr<tThread>>(0x7FFFFFFF));
  return thread_list;
}

} // namespace internal

tThread::tThread(bool anonymous, bool legion) :
  stop_signal(false),
  lock_stack(),
  id(internal::GetThreadCounter()++),
  name(internal::GetDefaultThreadName(id)),
  priority(cDEFAULT_PRIORITY),
  state(tState::RUNNING),
  self(this, internal::tThreadDeleter()),
  delete_on_completion(true),
  start_signal(false),
  monitor(*this),
  thread_list_ref(internal::GetThreadList()),
  locked_objects(),
  longevity(0),
  unknown_thread(true),
  wrapped_thread(),
  handle(pthread_self()),
  joining_threads(0)
{
  AddToThreadList();

  // see if we can obtain a thread name
  char name_buffer[1024];
  if (!pthread_getname_np(handle, name_buffer, 1023))
  {
    name_buffer[1023] = 0;
    if (strlen(name_buffer) > 0)
    {
      name = name_buffer;
    }
  }
}

tThread::tThread(const std::string& name) :
  stop_signal(false),
  lock_stack(),
  id(internal::GetThreadCounter()++),
  name(name.length() > 0 ? name : internal::GetDefaultThreadName(id)),
  priority(cDEFAULT_PRIORITY),
  state(tState::NEW),
  self(this, internal::tThreadDeleter()),
  delete_on_completion(false),
  start_signal(false),
  monitor(*this),
  thread_list_ref(internal::GetThreadList()),
  locked_objects(),
  longevity(0),
  unknown_thread(false),
  wrapped_thread(&Launch, this),
  handle(wrapped_thread.native_handle()),
  joining_threads(0)
{
  AddToThreadList();
  SetName(this->name);
}

tThread::~tThread()
{
  RRLIB_LOG_PRINT(eLL_DEBUG_VERBOSE_1, "Deleting thread ", this);

  // remove from thread list
  tLock lock(thread_list_ref->obj_mutex);
  assert(this != NULL);

  // remove thread from list
  for (size_t i = 0; i < thread_list_ref->vec.size(); i++)
  {
    std::shared_ptr<tThread> t = thread_list_ref->vec[i].lock();
    if (t.get() == this)
    {
      thread_list_ref->vec.erase(thread_list_ref->vec.begin() + i);
      break;
    }
    if (t.get() == NULL)   // remove empty entries
    {
      thread_list_ref->vec.erase(thread_list_ref->vec.begin() + i);
      i--;
    }
  }

  lock.Unlock();
  if (!unknown_thread)
  {
    if (tThread::CurrentThreadRaw() != this)
    {
      Join(); // we shouldn't delete anything while thread is still running
    }
    else if (wrapped_thread.joinable())
    {
      wrapped_thread.detach();
    }
  }

  for (auto rit = locked_objects.rbegin(); rit < locked_objects.rend(); ++rit)
  {
    (*rit).reset();
  }
}

void tThread::AddToThreadList()
{
  tLock lock(thread_list_ref->obj_mutex);
  thread_list_ref->vec.push_back(self);

  //printf("Creating thread %p %s\n", this, getName().getCString());
  RRLIB_LOG_PRINT(eLL_DEBUG_VERBOSE_1, "Creating thread ", this);
}

typedef rrlib::util::tSingletonHolder<boost::thread_specific_ptr<tThread>, rrlib::util::singleton::Longevity, internal::CreateCurThreadLocal> tCurThreadLocal;
static inline unsigned int GetLongevity(boost::thread_specific_ptr<tThread>*)
{
  return 0xFCCCCCCC; // should exit before allocation register
}

boost::thread_specific_ptr<tThread>& tThread::GetCurThreadLocal()
{
  return tCurThreadLocal::Instance();
}

std::string tThread::GetLogDescription() const
{
  std::ostringstream oss;
  oss << "Thread " << id << " '" << GetName() << "'";
  return oss.str();
}

void tThread::Join()
{
  if (unknown_thread)
  {
    RRLIB_LOG_PRINT(rrlib::logging::eLL_WARNING, "Operation not supported for threads of unknown origin.");
    return;
  }
  if (!wrapped_thread.joinable())
  {
    return;
  }
  if (CurrentThreadRaw() == this)
  {
    RRLIB_LOG_PRINT(rrlib::logging::eLL_DEBUG_WARNING, "Thread cannot join itself");
    return;
  }
  PreJoin();
  RRLIB_LOG_PRINT(rrlib::logging::eLL_DEBUG_VERBOSE_1, "Joining Thread");

  int joining = joining_threads.fetch_add(1);
  if (joining >= 1)
  {
    RRLIB_LOG_PRINT(rrlib::logging::eLL_DEBUG_WARNING, "Multiple threads are trying to join. Returning this thread without joining.");
    return;
  }
  if (wrapped_thread.joinable())
  {
    wrapped_thread.join();
  }
  RRLIB_LOG_PRINT(rrlib::logging::eLL_DEBUG_VERBOSE_1, "Joined Thread");
}

void tThread::Launch(tThread* thread_ptr)
{
  thread_ptr->Launcher();
}

void tThread::Launcher()
{
  //unsafe _FINROC_LOG_MESSAGE(eLL_DEBUG_VERBOSE_2, logDomain) << "Entering";
  cur_thread = this;
  tLock l(*this);
  state = tState::PREPARE_RUNNING;
  //unsafe _FINROC_LOG_MESSAGE(eLL_DEBUG_VERBOSE_2, logDomain) << "Locked";
  //curThread = threadPtr;
  GetCurThreadLocal().reset(this);
  //unsafe _FINROC_LOG_MESSAGE(eLL_DEBUG_VERBOSE_2, logDomain) << "ThreadLocal set";

  // wait for start signal
  while ((!(start_signal)) && (!(stop_signal)))
  {
    monitor.Wait(l);
  }

  // run thread?
  state = tState::RUNNING;
  if (start_signal && (!(stop_signal)))
  {
    try
    {
      l.Unlock();
      RRLIB_LOG_PRINT(eLL_DEBUG, "Thread started");
      Run();
      RRLIB_LOG_PRINT(eLL_DEBUG, "Thread exited normally");
    }
    catch (const std::exception& e)
    {
      RRLIB_LOG_PRINT(eLL_ERROR, "Thread exited because of exception: ", e.what());
    }
  }

  RRLIB_LOG_PRINT(eLL_DEBUG_VERBOSE_2, "Exiting");

  l.Unlock();
  // Do this BEFORE thread is removed from list - to ensure this is done StopThreads
  try
  {
    for (auto rit = locked_objects.rbegin(); rit < locked_objects.rend(); ++rit)
    {
      (*rit).reset();
    }
  }
  catch (const std::exception& e)
  {
    RRLIB_LOG_PRINT(eLL_ERROR, "Thread encountered exception during cleanup: ", e.what());
  }
}

void tThread::LockObject(std::shared_ptr<void> obj)
{
  tLock(*this);
  locked_objects.push_back(obj);
}

void tThread::PreJoin()
{
  RRLIB_LOG_PRINT(eLL_DEBUG_VERBOSE_2, "Entering");
  tLock l(*this);
  RRLIB_LOG_PRINT(eLL_DEBUG_VERBOSE_2, "Locked");
  if (state == tState::PREPARE_RUNNING || state == tState::NEW)
  {
    RRLIB_LOG_PRINT(eLL_DEBUG_VERBOSE_2, "Notifying");
    stop_signal = true;
    monitor.Notify(l);
    RRLIB_LOG_PRINT(eLL_DEBUG_VERBOSE_2, "Notified");
  }
  RRLIB_LOG_PRINT(eLL_DEBUG_VERBOSE_2, "Leaving");
}

void tThread::SetName(const std::string& name)
{
  this->name = name;
  pthread_setname_np(handle, name.substr(0, 15).c_str());
}

void tThread::SetRealtime()
{
  struct sched_param param;
  param.sched_priority = 49;
  if (pthread_setschedparam(handle, SCHED_FIFO, &param))
  {
    //printf("Failed making thread a real-time thread. Possibly current user has insufficient rights.\n");
    RRLIB_LOG_PRINT(eLL_ERROR, "Failed making thread a real-time thread. Possibly current user has insufficient rights.");
  }
}


void tThread::Sleep(const rrlib::time::tDuration& sleep_for, bool use_application_time, rrlib::time::tTimestamp wait_until)
{
  rrlib::time::tTimeMode time_mode = rrlib::time::GetTimeMode();
  tThread& t = *CurrentThreadRaw();
  if (time_mode == rrlib::time::tTimeMode::SYSTEM_TIME || (!use_application_time))
  {
    if (sleep_for <= std::chrono::milliseconds(500))
    {
      std::this_thread::sleep_for(sleep_for);
    }
    else
    {
      tLock l(t);
      t.monitor.Wait(l, sleep_for, use_application_time, wait_until);
    }
  }
  else if (time_mode == rrlib::time::tTimeMode::CUSTOM_CLOCK)
  {
    tLock l(t);
    t.monitor.Wait(l, sleep_for, use_application_time, wait_until);
  }
  else
  {
    assert(time_mode == rrlib::time::tTimeMode::STRETCHED_SYSTEM_TIME);
    tLock l(t);
    rrlib::time::tDuration system_duration = rrlib::time::ToSystemDuration(sleep_for);
    if (system_duration > std::chrono::milliseconds(20))
    {
      t.monitor.Wait(l, system_duration, use_application_time, wait_until);
    }
    else
    {
      l.Unlock();
      std::this_thread::sleep_for(system_duration);
    }
  }
}

void tThread::Start()
{
  RRLIB_LOG_PRINT(eLL_DEBUG_VERBOSE_2, "Entering");
  RRLIB_LOG_PRINT(eLL_DEBUG_VERBOSE_2, "PreMutex");
  tLock l(*this);
  RRLIB_LOG_PRINT(eLL_DEBUG_VERBOSE_2, "Locked");
  start_signal = true;
  monitor.Notify(l);
  RRLIB_LOG_PRINT(eLL_DEBUG_VERBOSE_2, "Notified thread");
}

void tThread::StopThread()
{
  if (unknown_thread)
  {
    RRLIB_LOG_PRINT(rrlib::logging::eLL_WARNING, "Operation not supported for threads of unknown origin.");
    return;
  }

  // default implementation - possibly sufficient for some thread classes
  tLock l(*this);
  stop_signal = true;
  monitor.Notify(l);
}

bool tThread::StopThreads(bool query_only)
{
  volatile static bool stopping_threadz = false;
  if (stopping_threadz || query_only)   // We don't do this twice
  {
    return stopping_threadz;
  }
  stopping_threadz = true;
  const char*(*GetLogDescription)() = GetLogDescriptionStatic;
  RRLIB_LOG_PRINT(eLL_USER, "Stopping all threads");

  tLock lock(internal::GetThreadList()->obj_mutex);
  std::vector<std::weak_ptr<tThread>> current_threads_unordered;
  std::vector<std::weak_ptr<tThread>> current_threads;
  current_threads_unordered = internal::GetThreadList()->vec;
  lock.Unlock();

  // Sort threads according to longevity
  int64_t last_longevity = -1;
  while (true)
  {
    int64_t min_longevity = 0xFFFFFFFFFFLL;
    for (size_t i = 0; i < current_threads_unordered.size(); i++)
    {
      std::shared_ptr<tThread> t = current_threads_unordered[i].lock();
      if (t && t->longevity < min_longevity && t->longevity > last_longevity)
      {
        min_longevity = t->longevity;
      }
    }
    if (min_longevity == 0xFFFFFFFFFFLL)
    {
      break;
    }

    // Copy to new list
    for (size_t i = 0; i < current_threads_unordered.size(); i++)
    {
      std::shared_ptr<tThread> t = current_threads_unordered[i].lock();
      if (t && t->longevity == min_longevity)
      {
        current_threads.push_back(current_threads_unordered[i]);
      }
    }

    last_longevity = min_longevity;
  }

  // Delete threads in now correct order
  for (size_t i = 0; i < current_threads.size(); i++)
  {
    std::weak_ptr<tThread> thread = current_threads[i];
    std::shared_ptr<tThread> t = thread.lock();
    if (t && t.get() != CurrentThreadRaw())
    {
      if (t->unknown_thread)
      {
        RRLIB_LOG_PRINT(rrlib::logging::eLL_WARNING, "Do not know how to stop thread '", t->GetLogDescription(), "' of unknown origin.");
        continue;
      }

      RRLIB_LOG_PRINT(eLL_DEBUG, "Stopping thread '", t->GetLogDescription(), "'");
      tLock l(*t);
      t->stop_signal = true;
      if (!t->start_signal)
      {
        t->monitor.Notify(l);
      }
      else
      {
        t->monitor.Notify(l);
        l.Unlock();
        t->StopThread();
        t->Join();
      }
    }
  }

  return true;
}

void tThread::Yield()
{
  std::this_thread::yield();
}

//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}