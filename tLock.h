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
 * (Wraps std::unique_lock for different mutex types.
 *  Additionally enforces correct lock order if #define
 *  RRLIB_THREAD_ENFORCE_LOCK_ORDER is enabled)
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
//! Unique Lock
/*!
 * Lock for all mutex classes in rrlib_thread
 *
 * (Wraps std::unique_lock for different mutex types.
 *  Additionally enforces correct lock order if #define
 *  RRLIB_THREAD_ENFORCE_LOCK_ORDER is enabled)
 */
class tLock : private util::tNoncopyable
{
  friend class internal::tLockStack;

//----------------------------------------------------------------------
// Public methods and typedefs
//----------------------------------------------------------------------
public:

#ifndef RRLIB_SINGLE_THREADED

  /*! Creates lock not related to any mutex (cannot be locked) */
  explicit tLock() :
    simple_lock(),
    recursive_lock(),
    locked_ordered(NULL),
    locked_simple(NULL)
  {}

  /*!
   * \param mutex Mutex to lock
   * \param immediately_lock Immediately lock mutex on construction of this lock?
   * (If this is false, Lock() or TryLock() can be called later to acquire lock)
   */
  template <typename TMutex>
  inline explicit tLock(const TMutex& mutex, bool immediately_lock = true) :
    simple_lock(),
    recursive_lock(),
    locked_ordered(GetLockedOrdered(mutex)),
    locked_simple(GetLockedSimple(mutex))
  {
    if (immediately_lock)
    {
      Lock(mutex);
    }
  }

  /*! move constructor */
  explicit tLock(tLock && other) :
    simple_lock(),
    recursive_lock(),
    locked_ordered(NULL),
    locked_simple(NULL)
  {
    std::swap(simple_lock, other.simple_lock);
    std::swap(recursive_lock, other.recursive_lock);
    std::swap(locked_ordered, other.locked_ordered);
    std::swap(locked_simple, other.locked_simple);
  }

  /*! move assignment */
  tLock& operator=(tLock && other)
  {
    std::swap(simple_lock, other.simple_lock);
    std::swap(recursive_lock, other.recursive_lock);
    std::swap(locked_ordered, other.locked_ordered);
    std::swap(locked_simple, other.locked_simple);
    return *this;
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
   * Lock (again)
   */
  inline void Lock()
  {
    assert(((!simple_lock.owns_lock()) && (!(recursive_lock.owns_lock()))) && "Unlock before calling lock()");
    if (locked_simple)
    {
      Lock(*locked_simple);
    }
    else if (locked_ordered)
    {
      Lock(static_cast<const tRecursiveMutex&>(*locked_ordered));
    }
  }

  /*!
   * Tries to acquire lock on mutex (does not block)
   *
   * \return True, if lock could be acquired
   */
  inline bool TryLock()
  {
    assert(((!simple_lock.owns_lock()) && (!(recursive_lock.owns_lock()))) && "Unlock before calling lock()");
    if (locked_simple)
    {
      return TryLock(*locked_simple);
    }
    else if (locked_ordered)
    {
      return TryLock(static_cast<const tRecursiveMutex&>(*locked_ordered));
    }
    return true;
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

#else

  tLock() {}

  template <typename T>
  tLock(const T& mutex) {}

  template <typename T1, typename T2>
  tLock(const T& mutex, T2 parameter) {}

  ~tLock() {}

#endif


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


  // Internal helper methods to handle different variants of mutexes uniformly in constructors

  inline const tOrderedMutexBaseClass* GetLockedOrdered(const tMutex& mutex)
  {
    return NULL;
  }
  inline const tOrderedMutexBaseClass* GetLockedOrdered(const tNoMutex& mutex)
  {
    return NULL;
  }
  inline const tOrderedMutexBaseClass* GetLockedOrdered(const tOrderedMutex& mutex)
  {
    return &mutex;
  }
  inline const tOrderedMutexBaseClass* GetLockedOrdered(const tRecursiveMutex& mutex)
  {
    return &mutex;
  }
  inline const tMutex* GetLockedSimple(const tMutex& mutex)
  {
    return &mutex;
  }
  inline const tMutex* GetLockedSimple(const tNoMutex& mutex)
  {
    return NULL;
  }
  inline const tMutex* GetLockedSimple(const tOrderedMutex& mutex)
  {
    return &mutex;
  }
  inline const tMutex* GetLockedSimple(const tRecursiveMutex& mutex)
  {
    return NULL;
  }

  inline void Lock(const tNoMutex& mutex) {}
  inline void Lock(const tMutex& mutex)
  {
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
    internal::tLockStack::Push(this);
#endif
    simple_lock = std::unique_lock<std::mutex>(mutex.wrapped);
  }
  inline void Lock(const tRecursiveMutex& mutex)
  {
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
    internal::tLockStack::Push(this);
#endif
    recursive_lock = std::unique_lock<std::recursive_mutex>(mutex.wrapped);
  }

  inline bool TryLock(const tNoMutex& mutex)
  {
    return true;
  }
  inline bool TryLock(const tMutex& mutex)
  {
    simple_lock = std::unique_lock<std::mutex>(mutex.wrapped, std::defer_lock_t());
    bool locked = simple_lock.try_lock();
    if (locked)
    {
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
      internal::tLockStack::Push(this);
#endif
    }
    return locked;
  }
  inline bool TryLock(const tRecursiveMutex& mutex)
  {
    recursive_lock = std::unique_lock<std::recursive_mutex>(mutex.wrapped, std::defer_lock_t());
    bool locked = recursive_lock.try_lock();
    if (locked)
    {
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER
      internal::tLockStack::Push(this);
#endif
    }
    return locked;
  }

#endif


};

//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}


#endif
