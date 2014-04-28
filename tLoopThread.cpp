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
/*!\file    rrlib/thread/tLoopThread.cpp
 *
 * \author  Max Reichardt
 *
 * \date    2012-07-05
 *
 */
//----------------------------------------------------------------------
#include "rrlib/thread/tLoopThread.h"

//----------------------------------------------------------------------
// External includes (system with <>, local with "")
//----------------------------------------------------------------------

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
const bool tLoopThread::cDISPLAYWARNINGS;

//----------------------------------------------------------------------
// Implementation
//----------------------------------------------------------------------

tLoopThread::tLoopThread(rrlib::time::tDuration default_cycle_time, bool use_application_time, bool warn_on_cycle_time_exceed, bool pause_on_startup) :
  pause_signal(pause_on_startup),
  cycle_time(default_cycle_time),
  use_application_time(use_application_time),
  warn_on_cycle_time_exceed(warn_on_cycle_time_exceed),
  last_cycle_time(),
  last_cycle_start(rrlib::time::cNO_TIME),
  last_wait(rrlib::time::tDuration::zero())
{
}

void tLoopThread::ContinueThread()
{
  tLock l(*this);
  pause_signal = false;
  GetMonitor().Notify(l);
}

void tLoopThread::MainLoop()
{
  while (!IsStopSignalSet())
  {
    if (pause_signal.load(std::memory_order_relaxed))
    {
      last_cycle_start = rrlib::time::cNO_TIME;
      tLock l(*this);
      GetMonitor().Wait(l);
      continue;
    }

    if (last_cycle_start != rrlib::time::cNO_TIME)
    {
      // copy atomics to local variables
      rrlib::time::tDuration cycle_time = this->cycle_time.Load();
      bool local_use_application_time = use_application_time.load(std::memory_order_relaxed);

      // wait
      rrlib::time::tTimestamp now = local_use_application_time ? rrlib::time::Now() : rrlib::time::tBaseClock::now();
      rrlib::time::tDuration last_cycle_time_tmp = now - last_cycle_start;
      if (last_cycle_time_tmp.count() < 0)
      {
        if (-last_cycle_time_tmp <= cycle_time)
        {
          RRLIB_LOG_PRINT(WARNING, "Early thread wakeup detected");
        }
        else
        {
          RRLIB_LOG_PRINT(WARNING, "Clock inconsistency detected: According to clock, current cycle started ", rrlib::time::ToString(-last_cycle_time_tmp), " before it should have. This would have been before the last cycle.");
        }

        last_cycle_start = now;
        if (last_wait > rrlib::time::tDuration::zero())
        {
          RRLIB_LOG_PRINT(WARNING, "Waiting for ", rrlib::time::ToString(last_wait), ", as in last cycle.");
          Sleep(last_wait, local_use_application_time);
        }
        else
        {
          RRLIB_LOG_PRINT(WARNING, "Not waiting at all. As it appears, this thread has never waited yet.");
        }
      }
      else
      {
        last_cycle_time.Store(last_cycle_time_tmp);
        rrlib::time::tDuration wait_for_x = cycle_time - last_cycle_time_tmp;
        if (wait_for_x < rrlib::time::tDuration::zero())
        {
          if (warn_on_cycle_time_exceed && cDISPLAYWARNINGS)
          {
            RRLIB_LOG_PRINT(WARNING, "Couldn't keep up cycle time (", rrlib::time::ToString(-wait_for_x), " too long)");
          }
          last_cycle_start = now;
        }
        else
        {
          last_wait = wait_for_x;
          if (wait_for_x > rrlib::time::tDuration::zero())
          {
            Sleep(wait_for_x, local_use_application_time, last_cycle_start + cycle_time);
          }
          last_cycle_start += cycle_time;
        }
      }
    }
    else
    {
      last_cycle_start = use_application_time.load(std::memory_order_relaxed) ? rrlib::time::Now() : rrlib::time::tBaseClock::now();
    }

    MainLoopCallback();
  }
}

void tLoopThread::Run()
{
  // Start main loop
  MainLoop();
}

void tLoopThread::SetUseApplicationTime(bool use_application_time)
{
  //assert(&CurrentThread() == this && "Please only call from this thread");
  bool last_value = this->use_application_time.exchange(use_application_time);
  if (last_value != use_application_time)
  {
    last_cycle_start = rrlib::time::cNO_TIME;
  }
}


//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}
