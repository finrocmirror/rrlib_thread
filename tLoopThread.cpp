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
      rrlib::time::tTimestamp now = rrlib::time::Now();
      rrlib::time::tDuration last_cycle_time_tmp = now - last_cycle_start;
      last_cycle_time.Store(last_cycle_time_tmp);
      rrlib::time::tDuration wait_for_x = cycle_time - last_cycle_time_tmp;
      if (wait_for_x < rrlib::time::tDuration::zero() && warn_on_cycle_time_exceed && cDISPLAYWARNINGS)
      {
        //System.err.println("warning: Couldn't keep up cycle time (" + (-waitForX) + " ms too long)");
        RRLIB_LOG_PRINT(rrlib::logging::eLL_WARNING, "Couldn't keep up cycle time (", rrlib::time::ToString(-wait_for_x), " too long)");
      }
      else if (wait_for_x > cycle_time)
      {
        RRLIB_LOG_PRINT(rrlib::logging::eLL_WARNING, "Clock inconsistency detected: Last cycle started \"after\" this cycle. This would mean we'd have to wait for ", rrlib::time::ToString(wait_for_x), " now.");
        if (last_wait > rrlib::time::tDuration::zero())
        {
          RRLIB_LOG_PRINT(rrlib::logging::eLL_WARNING, "Waiting for ", rrlib::time::ToString(last_wait), ", as in last cycle, instead.");
          Sleep(last_wait, local_use_application_time);
        }
        else
        {
          RRLIB_LOG_PRINT(rrlib::logging::eLL_WARNING, "Not waiting at all. As it appears, this thread has never waited yet.");
        }
      }
      else if (wait_for_x > rrlib::time::tDuration::zero())
      {
        last_wait = wait_for_x;
        Sleep(wait_for_x, local_use_application_time, last_cycle_start + cycle_time);
      }
      last_cycle_start += cycle_time;
      if (wait_for_x < rrlib::time::tDuration::zero())
      {
        last_cycle_start = rrlib::time::Now();
      }
    }
    else
    {
      last_cycle_start = rrlib::time::Now();
    }

    MainLoopCallback();
  }
}

void tLoopThread::Run()
{
  try
  {
    //stopSignal = false; // this may lead to unintended behaviour

    // Start main loop
    MainLoop();
  }
  catch (const std::exception& e)
  {
    RRLIB_LOG_PRINT(rrlib::logging::eLL_DEBUG_WARNING, "Uncaught Exception: ", e);
  }
}

void tLoopThread::SetUseApplicationTime(bool use_application_time)
{
  //assert(&CurrentThread() == this && "Please only call from this thread");
  bool last_value = this->use_application_time.exchange(use_application_time);
  if (last_value != use_application_time)
  {
    last_cycle_start != rrlib::time::cNO_TIME;
  }
}


//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}
