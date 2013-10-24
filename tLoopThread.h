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
/*!\file    rrlib/thread/tLoopThread.h
 *
 * \author  Max Reichardt
 *
 * \date    2012-07-05
 *
 * \brief   Contains tLoopThread
 *
 * \b tLoopThread
 *
 * A Thread that calls a callback function with a specified rate.
 *
 */
//----------------------------------------------------------------------
#ifndef __rrlib__thread__tLoopThread_h__
#define __rrlib__thread__tLoopThread_h__

//----------------------------------------------------------------------
// External includes (system with <>, local with "")
//----------------------------------------------------------------------
#include "rrlib/time/tAtomicDuration.h"

//----------------------------------------------------------------------
// Internal includes with ""
//----------------------------------------------------------------------
#include "rrlib/thread/tThread.h"

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
// Class declaration
//----------------------------------------------------------------------
//! Loop Thread.
/*!
 * A Thread that calls a callback function with a specified rate.
 */
class tLoopThread : public tThread
{

//----------------------------------------------------------------------
// Public methods and typedefs
//----------------------------------------------------------------------
public:

  /*!
   * \param default_cycle_time Cycle time with which callback function is called
   * \param use_application_time Use "application time" (see rrlib/util/time.h) instead of system time?
   * \param warn_on_cycle_time_exceed Display warning, if cycle time is exceeded?
   * \param pause_on_startup Pause Signal set at startup of this thread?
   */
  explicit tLoopThread(rrlib::time::tDuration default_cycle_time, bool use_application_time = true, bool warn_on_cycle_time_exceed = false, bool pause_on_startup = false);

  /*!
   * Resume Thread;
   */
  void ContinueThread();

  /*!
   * \return Start time of current cycle (always smaller than rrlib::time::Now())
   */
  inline rrlib::time::tTimestamp GetCurrentCycleStartTime() const
  {
    assert(&CurrentThread() == this && "Please only call from this thread");
    return last_cycle_start;
  }

  /*!
   * \return Current Cycle time with which callback function is called
   */
  inline rrlib::time::tDuration GetCycleTime() const
  {
    return cycle_time.Load();
  }

  /*!
   * \return Time spent in last call to MainLoopCallback()
   */
  inline rrlib::time::tDuration GetLastCycleTime() const
  {
    return last_cycle_time.Load();
  }

  /*!
   * \return Is Thread currently paused?
   */
  inline bool IsPausing() const
  {
    return pause_signal;
  }

  /*!
   * \return Is thread currently running? (and not paused)
   */
  inline bool IsRunning() const
  {
    return IsAlive() && !IsPausing();
  }

  /*!
   * \return Is this thread using "application time" instead of system time (see rrlib/util/time.h)?
   */
  inline bool IsUsingApplicationTime() const
  {
    return use_application_time;
  }

  /*!
   * Callback function that is called with the specified rate
   */
  virtual void MainLoopCallback() = 0;

  /*!
   * Pause Thread.
   */
  inline void PauseThread()
  {
    pause_signal = true;
  }

  virtual void Run();

  /*!
   * \param cycle_time New Cycle time with which callback function is called
   */
  inline void SetCycleTime(const rrlib::time::tDuration& cycle_time)
  {
    this->cycle_time.Store(cycle_time);
  }

  /*!
   * \param use_application_time Use "application time" (see rrlib/util/time.h) instead of system time?
   *
   * This will not have any effect on Sleep() and Wait() operations this thread might be executing.
   *
   * (This method is preferably called from this thread - to avoid issues in case
   *  thread is currently performing operations with another time base)
   */
  void SetUseApplicationTime(bool use_application_time);

//----------------------------------------------------------------------
// Private fields and methods
//----------------------------------------------------------------------
private:

  // TODO: For optimization, we could put the following three atomic variables in a single 64-bit atomic

  /*! Thread pauses if this flag is set */
  std::atomic<bool> pause_signal;

  /*! Cycle time with which callback function is called */
  rrlib::time::tAtomicDuration cycle_time;

  /*! Use "application time" (see rrlib/util/time.h) instead of system time? */
  std::atomic<bool> use_application_time;

  /*! Display warning, if cycle time is exceeded? */
  const bool warn_on_cycle_time_exceed;

  /*! Display warnings on console? */
  static const bool cDISPLAYWARNINGS = false;

  /*! Time spent in last call to MainLoopCallback() */
  rrlib::time::tAtomicDuration last_cycle_time;

  /*! Start time of last cycle */
  rrlib::time::tTimestamp last_cycle_start;

  /*! Time thread waited in last loop */
  rrlib::time::tDuration last_wait;


  /*!
   * The main loop
   */
  void MainLoop();

};

//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}


#endif
