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
/*!\file    rrlib/thread/tConditionVariable.h
 *
 * \author  Max Reichardt
 *
 * \date    2012-06-12
 *
 * \brief   Contains tConditionVariable
 *
 * \b tConditionVariable
 *
 * Condition variable whose timeout can be specified in "application time".
 * If "application time" stretching factor changes during wait, timeout is adjusted (!)
 * It also works with custom clocks.
 */
//----------------------------------------------------------------------
#ifndef rrlib__thread__tConditionVariable_h__
#define rrlib__thread__tConditionVariable_h__

//----------------------------------------------------------------------
// External includes (system with <>, local with "")
//----------------------------------------------------------------------
#include "rrlib/time/time.h"
#include <condition_variable>

//----------------------------------------------------------------------
// Internal includes with ""
//----------------------------------------------------------------------
#include "rrlib/thread/tLock.h"

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
//! Condition variable for "application time".
/*!
 * Condition variable whose timeout can be specified in "application time".
 * If "application time" stretching factor changes during wait, timeout is adjusted (!)
 * It also works with custom clocks.
 * There is, however, a limitation:
 * Only one thread may wait on this condition variable using an "application time"-timeout.
 *
 * This class allows to use Wait() and Notify(), NotifyAll() somewhat similar as with Java objects.
 *
 * A Mutex needs to be associated to objects of this class in the constructor.
 * This mutex needs to be acquired for every operation.
 *
 * Note, that wait-timeout is always system time using this class.
 * If you need "application time" (that is possibly adjusted during wait due to stretching factor change; see rrlib/time/time.h), use tThread::Wait().
 */
class tConditionVariable : private util::tNoncopyable
{

//----------------------------------------------------------------------
// Public methods and typedefs
//----------------------------------------------------------------------
public:

  explicit tConditionVariable(tMutex& mutex);

  ~tConditionVariable();

  /*!
   * Wakes up a single thread that is waiting on this object's monitor.
   *
   * \param l Acquired lock on associated mutex
   */
  void Notify(tLock& l);

  /*!
   * Wakes up all threads that are waiting on this monitor.
   *
   * \param l Acquired lock on associated mutex
   */
  void NotifyAll(tLock& l);

  /*!
   * The current thread waits until another thread calls the notify or notifyAll methods
   *
   * \param l Acquired lock on associated mutex
   */
  void Wait(tLock& l);

  /*!
   * The current thread waits until another thread calls the notify or notifyAll methods - or the timeout expires.
   * If "application time" stretching factor changes during wait, timeout is adjusted (!)
   * It also works with custom clocks.
   *
   * \param l Acquired lock on associated mutex
   * \param wait_for Duration to wait
   * \param use_application_time Use application time?
   * \param wait_until Time point until to wait (optional). If specified, this function won't need call clock's now() in case application time is used.
   */
  void Wait(tLock& l, const rrlib::time::tDuration& wait_for, bool use_application_time, rrlib::time::tTimestamp wait_until = rrlib::time::cNO_TIME);

//----------------------------------------------------------------------
// Private fields and methods
//----------------------------------------------------------------------
private:

  friend class tTimeStretchingListenerImpl;

  /*! Mutex that needs to be acquired before doing anything with this monitor */
  tMutex& mutex;

  /*! Wrapped boost condition variable */
  std::condition_variable wrapped;

  /*! Has condition variable been registered for application time listening? */
  bool registered_in_list;

  /*! Number of threads waiting on monitor */
  int waiting_on_monitor;

  /*! "Application time" until thread is waiting (cNO_TIME if no thread is waiting - or only waiting for system time) */
  rrlib::time::tTimestamp waiting_until_application_time;

  /*! "Application time" for which thread is waiting (undefined if no thread is waiting - or only waiting for system time) */
  rrlib::time::tDuration waiting_for_application_duration;

  /*! Flag set when time scaling factor changes during wait */
  bool time_scaling_factor_changed;

  /*! Flag set when threads waiting on monitor are notified */
  bool notified;

  /*!
   * \return Has condition variable been correctly acquired to perform operation condition variable?
   */
  bool ConditionVariableLockCorrectlyAcquired(const tLock& l) const;
};

//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}


#endif
