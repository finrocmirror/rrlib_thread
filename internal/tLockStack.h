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
/*!\file    rrlib/thread/internal/tLockStack.h
 *
 * \author  Max Reichardt
 *
 * \date    2012-07-04
 *
 * \brief   Contains tLockStack
 *
 * \b tLockStack
 *
 * Provides access to thread-local Lock stack (contains all locks that were acquired by this thread)
 * This is required for enforcing total ordering on locks efficiently.
 *
 */
//----------------------------------------------------------------------
#ifndef __rrlib__thread__internal__tLockStack_h__
#define __rrlib__thread__internal__tLockStack_h__

//----------------------------------------------------------------------
// External includes (system with <>, local with "")
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Internal includes with ""
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
class tLock;
class tConditionVariable;

namespace internal
{
//----------------------------------------------------------------------
// Class declaration
//----------------------------------------------------------------------
//! Lock stack
/*!
 * Provides access to thread-local Lock stack (contains all locks that were acquired by this thread)
 * This is required for enforcing total ordering on locks efficiently.
 */
class tLockStack
{
#ifndef RRLIB_SINGLE_THREADED
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER

  friend class rrlib::thread::tConditionVariable;
  friend class rrlib::thread::tLock;

//----------------------------------------------------------------------
// Public methods and typedefs
//----------------------------------------------------------------------
public:

  /*! Dump stack (for debugging) */
  static void DumpStack();

//----------------------------------------------------------------------
// Private fields and methods
//----------------------------------------------------------------------
private:

  /*!
   * Is it okay to call a condition variable's method?
   * (Current thread must hold its associated lock exactly once - and last added)
   *
   * \param The condition variable's associated lock
   */
  static bool ConditionVariableLockCorrectlyAcquired(const tLock& lock);

  /*! Pushes lock on to thread's stack */
  static void Push(const tLock* lock);

  /*! Pops last lock off thread's stack */
  static const tLock* Pop();

#endif
#endif
};

//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}
}


#endif
