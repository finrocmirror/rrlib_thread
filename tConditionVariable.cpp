//
// You received this file as part of RRLib
// Robotics Research Library
//
// Copyright (C) Finroc GbR (finroc.org)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
//----------------------------------------------------------------------
/*!\file    rrlib/thread/tConditionVariable.cpp
 *
 * \author  Max Reichardt
 *
 * \date    2012-06-12
 *
 */
//----------------------------------------------------------------------
#include "rrlib/thread/tConditionVariable.h"

//----------------------------------------------------------------------
// External includes (system with <>, local with "")
//----------------------------------------------------------------------
#include "rrlib/design_patterns/singleton.h"
#include "rrlib/time/tTimeStretchingListener.h"
#include "rrlib/logging/messages.h"

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

#ifndef RRLIB_SINGLE_THREADED
typedef rrlib::design_patterns::tSingletonHolder<std::vector<tConditionVariable*>> tConditionVariableListSingleton;
static std::vector<tConditionVariable*>* GetConditionVariableList()
{
  try
  {
    return &tConditionVariableListSingleton::Instance();
  }
  catch (const std::logic_error& e)
  {}
  return NULL;
}

class tTimeStretchingListenerImpl : public rrlib::time::tTimeStretchingListener
{
  rrlib::time::tTimeMode old_mode;

  virtual void TimeChanged(const rrlib::time::tTimestamp& current_time) override
  {
    if (!GetConditionVariableList())
    {
      return;
    }

    // we don't need any locks, because TimeChanged is called with rrlib::time::mutex acquired
    std::vector<tConditionVariable*>& l = *GetConditionVariableList();
    for (auto it = l.begin(); it < l.end(); it++)
    {
      tLock l((*it)->mutex);
      if ((*it)->waiting_on_monitor && (*it)->waiting_until_application_time != rrlib::time::cNO_TIME)
      {
        (*it)->time_scaling_factor_changed = true;
        if (current_time >= (*it)->waiting_until_application_time)
        {
          (*it)->wrapped.notify_all();
        }
        else
        {
          rrlib::time::tDuration wait_for = (*it)->waiting_until_application_time - current_time;
          if (wait_for > (*it)->waiting_for_application_duration)
          {
            // clock inconsistency
            RRLIB_LOG_PRINT(DEBUG_WARNING, "Detected clock inconsistency. New timestamp is more than ",
                            rrlib::time::ToIsoString(wait_for - (*it)->waiting_for_application_duration), " in the past.");
            (*it)->waiting_for_application_duration = (*it)->waiting_for_application_duration / 2;
            RRLIB_LOG_PRINT(DEBUG_WARNING, "Recovering by waiting another ", rrlib::time::ToString((*it)->waiting_for_application_duration), " (half the original timeout).");

            (*it)->waiting_until_application_time = current_time - (*it)->waiting_for_application_duration;
          }
          else
          {
            (*it)->waiting_for_application_duration = wait_for;
          }
        }
      }
    }
  }

  virtual void TimeModeChanged(rrlib::time::tTimeMode new_mode) override
  {
    if (!GetConditionVariableList())
    {
      return;
    }

    if (old_mode == rrlib::time::tTimeMode::CUSTOM_CLOCK && new_mode != old_mode)
    {
      // we don't need any locks, because TimeChanged is called with rrlib::time::mutex acquired
      std::vector<tConditionVariable*>& l = *GetConditionVariableList();
      for (auto it = l.begin(); it < l.end(); it++)
      {
        tLock l((*it)->mutex);
        if ((*it)->waiting_on_monitor && (*it)->waiting_until_application_time != rrlib::time::cNO_TIME)
        {
          (*it)->wrapped.notify_all();
        }
      }
    }
    old_mode = new_mode;
  }

  virtual void TimeStretchingFactorChanged(bool app_time_faster) override
  {
    if (!GetConditionVariableList())
    {
      return;
    }

    // we don't need any locks, because TimeChanged is called with rrlib::time::mutex acquired
    std::vector<tConditionVariable*>& l = *GetConditionVariableList();
    for (auto it = l.begin(); it < l.end(); it++)
    {
      tLock l((*it)->mutex);
      if ((*it)->waiting_on_monitor && (*it)->waiting_until_application_time != rrlib::time::cNO_TIME)
      {
        (*it)->time_scaling_factor_changed = true;
        if (app_time_faster)
        {
          (*it)->wrapped.notify_one();
        }
      }
    }
  }

public:

  tTimeStretchingListenerImpl() : old_mode(rrlib::time::GetTimeMode()) {}
};

static tTimeStretchingListenerImpl time_stretching_listener;

/*!
 * This object increments value while it exists
 */
struct tTemporalIncrement : private util::tNoncopyable
{
  int& value;
  tTemporalIncrement(int& value) : value(value)
  {
    value++;
  }

  ~tTemporalIncrement()
  {
    value--;
  }
};

tConditionVariable::tConditionVariable(tMutex& mutex) :
  mutex(mutex),
  wrapped(),
  registered_in_list(false),
  waiting_on_monitor(0),
  waiting_until_application_time(rrlib::time::cNO_TIME),
  waiting_for_application_duration(-1),
  time_scaling_factor_changed(false),
  notified(false)
{}

tConditionVariable::~tConditionVariable()
{
  assert(waiting_on_monitor == 0);
  if (registered_in_list && GetConditionVariableList())
  {
    try
    {
      std::lock_guard<std::mutex> lock(rrlib::time::internal::tTimeMutex::Instance()); // we should not hold any critical locks - otherwise this will dead-lock
      std::vector<tConditionVariable*>& l = *GetConditionVariableList();
      l.erase(std::remove(l.begin(), l.end(), this), l.end());
    }
    catch (const std::logic_error& le) // can happen if tTimeMutex has already been destructed. In this case, we do not need to worry about unregistering anymore.
    {}
  }
}

bool tConditionVariable::ConditionVariableLockCorrectlyAcquired(const tLock& l) const
{
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
  return internal::tLockStack::ConditionVariableLockCorrectlyAcquired(l);
#else
  return l.IsLocked(mutex);
#endif
}

void tConditionVariable::Notify(tLock& l)
{
  assert(ConditionVariableLockCorrectlyAcquired(l));
  if (waiting_on_monitor)
  {
    notified = true;
    wrapped.notify_one();
  }
}

void tConditionVariable::NotifyAll(tLock& l)
{
  assert(ConditionVariableLockCorrectlyAcquired(l));
  if (waiting_on_monitor)
  {
    notified = true;
    wrapped.notify_all();
  }
}

void tConditionVariable::Wait(tLock& l)
{
  assert(ConditionVariableLockCorrectlyAcquired(l));
  tTemporalIncrement count_waiting_thread(waiting_on_monitor);
  wrapped.wait(l.GetSimpleLock());
}

void tConditionVariable::Wait(tLock& l, const rrlib::time::tDuration& wait_for, bool use_application_time, rrlib::time::tTimestamp wait_until)
{
  assert(ConditionVariableLockCorrectlyAcquired(l));
  tTemporalIncrement count_waiting_thread(waiting_on_monitor);

  if (!use_application_time)
  {
    wrapped.wait_for(l.GetSimpleLock(), wait_for);
  }
  else
  {
    // Put condition variable in list
    if (!registered_in_list)
    {
      try
      {
        std::lock_guard<std::mutex> l(rrlib::time::internal::tTimeMutex::Instance()); // this won't dead-lock, because this condition variable is not in listener list yet
        GetConditionVariableList()->push_back(this);
        registered_in_list = true;
      }
      catch (const std::logic_error&) // tTimeMutex no longer exists
      {
        RRLIB_LOG_PRINT(DEBUG_WARNING, "Won't wait after rrlibs have been (partly) destructed.");
        return;
      }
    }

    // Init variables
    time_scaling_factor_changed = false;
    notified = false;
    assert(waiting_until_application_time == rrlib::time::cNO_TIME && "Only one thread may wait on condition variable using 'application time' timeout.");
    if (wait_until == rrlib::time::cNO_TIME)
    {
      wait_until = rrlib::time::Now() + wait_for;
    }
    waiting_until_application_time = wait_until;
    waiting_for_application_duration = wait_for;

    // Wait...
    rrlib::time::tDuration system_duration = rrlib::time::ToSystemDuration(wait_for);
    rrlib::time::tTimeMode time_mode = rrlib::time::GetTimeMode();
    while (true)
    {
      if (time_mode == rrlib::time::tTimeMode::CUSTOM_CLOCK)
      {
        wrapped.wait(l.GetSimpleLock());
        time_mode = rrlib::time::GetTimeMode();
        if (notified || time_mode == rrlib::time::tTimeMode::CUSTOM_CLOCK)
        {
          break;
        }
      }
      else
      {
        wrapped.wait_for(l.GetSimpleLock(), system_duration);
        rrlib::time::tTimeMode new_time_mode = rrlib::time::GetTimeMode();
        if (notified || (!time_scaling_factor_changed && new_time_mode == time_mode))
        {
          break;
        }
        time_mode = new_time_mode;
      }
      rrlib::time::tTimestamp current_app_time = rrlib::time::Now();
      if (current_app_time >= wait_until)
      {
        break;
      }

      // ok... so we need to wait more
      assert(!notified);
      rrlib::time::tDuration waiting_for_application_duration_new = wait_until - current_app_time;
      if (waiting_for_application_duration_new > waiting_for_application_duration)
      {
        // clock inconsistency
        RRLIB_LOG_PRINT(DEBUG_WARNING, "Detected clock inconsistency. New timestamp is more than ",
                        rrlib::time::ToIsoString(waiting_for_application_duration_new - waiting_for_application_duration), " in the past.");
        waiting_for_application_duration = waiting_for_application_duration / 2;
        RRLIB_LOG_PRINT(DEBUG_WARNING, "Recovering by waiting another ", rrlib::time::ToString(waiting_for_application_duration), " (half the original timeout).");

        waiting_until_application_time = current_app_time - waiting_for_application_duration;
      }
      else
      {
        waiting_for_application_duration = waiting_until_application_time - current_app_time;
      }
      system_duration = rrlib::time::ToSystemDuration(waiting_for_application_duration);
      time_scaling_factor_changed = false;
    }

    waiting_until_application_time = rrlib::time::cNO_TIME;
  }
}
#else // RRLIB_SINGLE_THREADED

tConditionVariable::tConditionVariable(tMutex& mutex)
{}

tConditionVariable::~tConditionVariable()
{}

void tConditionVariable::Notify(tLock& l)
{}

void tConditionVariable::NotifyAll(tLock& l)
{}

void tConditionVariable::Wait(tLock& l)
{}

void tConditionVariable::Wait(tLock& l, const rrlib::time::tDuration& wait_for, bool use_application_time, rrlib::time::tTimestamp wait_until)
{}

#endif // RRLIB_SINGLE_THREADED

//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}
