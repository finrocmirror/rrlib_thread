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
/*!\file    rrlib/thread/tLock.h
 *
 * \author  Max Reichardt
 *
 * \date    2012-07-04
 *
 * \brief   Contains tLock
 *
 * \b tLock
 *
 * Lock for all mutex classes in rrlib_thread
 *
 */
//----------------------------------------------------------------------
#ifndef __rrlib__thread__tLock_h__
#define __rrlib__thread__tLock_h__

//----------------------------------------------------------------------
// External includes (system with <>, local with "")
//----------------------------------------------------------------------
#include <mutex>

//----------------------------------------------------------------------
// Internal includes with ""
//----------------------------------------------------------------------
#include "rrlib/thread/tNoMutex.h"
#include "rrlib/thread/tOrderedMutex.h"
#include "rrlib/thread/tRecursiveMutex.h"
#include "rrlib/thread/internal/tLockStack.h"

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

/*
#ifndef _FINROC_SYSTEM_INSTALLATION_PRESENT_
#ifndef NDEBUG
#define RRLIB_THREAD_ENFORCE_LOCK_ORDER
#endif
#endif
*/

//----------------------------------------------------------------------
// Class declaration
//----------------------------------------------------------------------
//! Lock
/*!
 * Lock for all mutex classes in rrlib_thread
 */
class tLock : boost::noncopyable
{
  friend class internal::tLockStack;

//----------------------------------------------------------------------
// Public methods and typedefs
//----------------------------------------------------------------------
public:

#ifndef RRLIB_SINGLE_THREADED

  explicit tLock(const tMutex& mutex) :
    simple_lock(mutex.wrapped),
    recursive_lock(),
    locked_ordered(NULL),
    locked_simple(&mutex)
  {
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
    internal::tLockStack::Push(this);
#endif
  }

  explicit tLock(const tNoMutex& mutex) :
    simple_lock(),
    recursive_lock(),
    locked_ordered(NULL),
    locked_simple(NULL)
  {
  }

  explicit tLock(const tOrderedMutex& mutex) :
    simple_lock(mutex.wrapped),
    recursive_lock(),
    locked_ordered(&mutex),
    locked_simple(&mutex)
  {
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
    internal::tLockStack::Push(this);
#endif
  }

  explicit tLock(const tRecursiveMutex& mutex) :
    simple_lock(),
    recursive_lock(mutex.wrapped),
    locked_ordered(&mutex),
    locked_simple()
  {
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
    internal::tLockStack::Push(this);
#endif
  }

  ~tLock()
  {
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
    if ((locked_simple && simple_lock.owns_lock()) || (locked_ordered && recursive_lock.owns_lock()))
    {
      bool ok = (internal::tLockStack::Pop() == this);
      assert(ok);
    }
#endif
  }

  /*!
   * \return Wrapped lock (only exists as long as this object!)
   */
  std::unique_lock<std::mutex>& GetSimpleLock()
  {
    return simple_lock;
  }

#else

  template <typename T>
  tLock(const T& mutex) {}

  ~tLock() {}

#endif

  /*!
   * \param mutex Mutex (either tOrderedMutex or tMutex)
   *
   * return Is specified mutex currently locked by this lock?
   */
  bool IsLocked(const tMutex& mutex) const
  {
    return &mutex == locked_simple && simple_lock.owns_lock();
  }

  /*!
   * Unlock/Release lock
   */
  void Unlock()
  {
    if (simple_lock.owns_lock())
    {
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
      bool ok = (internal::tLockStack::Pop() == this);
      assert(ok);
#endif
      simple_lock.unlock();
    }
    else if (recursive_lock.owns_lock())
    {
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
      bool ok = (internal::tLockStack::Pop() == this);
      assert(ok);
#endif
      recursive_lock.unlock();
    }
  }

  /*!
   * Lock (again)
   */
  void Lock()
  {
    assert(((!simple_lock.owns_lock()) && (!(recursive_lock.owns_lock()))) && "Unlock before calling lock()");
    if (locked_simple)
    {
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
      internal::tLockStack::Push(this);
#endif
      simple_lock.lock();
    }
    else if (locked_ordered)
    {
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
      internal::tLockStack::Push(this);
#endif
      recursive_lock.lock();
    }
  }

//----------------------------------------------------------------------
// Private fields and methods
//----------------------------------------------------------------------
private:

#ifndef RRLIB_SINGLE_THREADED

  /*! wrapped locks */
  std::unique_lock<std::mutex> simple_lock;
  std::unique_lock<std::recursive_mutex> recursive_lock;

  /*! Raw pointer(s) to mutex that was acquired by this lock */
  const tOrderedMutexBaseClass* locked_ordered;
  const tMutex* locked_simple;

#endif
};

//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}


#endif
