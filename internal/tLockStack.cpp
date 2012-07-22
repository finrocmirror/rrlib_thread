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
/*!\file    rrlib/thread/internal/tLockStack.cpp
 *
 * \author  Max Reichardt
 *
 * \date    2012-07-04
 *
 */
//----------------------------------------------------------------------
#include "rrlib/thread/internal/tLockStack.h"

//----------------------------------------------------------------------
// External includes (system with <>, local with "")
//----------------------------------------------------------------------
#include <vector>

//----------------------------------------------------------------------
// Internal includes with ""
//----------------------------------------------------------------------
#include "rrlib/thread/tLock.h"
#include "rrlib/thread/tThread.h"

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
namespace internal
{
#ifndef RRLIB_SINGLE_THREADED
#ifdef RRLIB_THREAD_ENFORCE_LOCK_ORDER

//----------------------------------------------------------------------
// Forward declarations / typedefs / enums
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Const values
//----------------------------------------------------------------------

/*! Maximum number of nested locks */
static const size_t cMAX_NESTED_LOCKS = 100;

//----------------------------------------------------------------------
// Implementation
//----------------------------------------------------------------------

/*!
 * Thread-Local Lock stack (contains all locks that were acquired by this thread)
 * (note that we do not use a std::vector here, because it cannot be used in __thread variables directly -
 * and it could pose problems during static (de)initialization)
 *
 * (Having this information entirely thread-local is more efficient and simpler than maintaining linked-lists across threads,
 * since variables would need to be volatile etc.)
 */
struct tLockStackData : boost::noncopyable
{
  /*! Stack entries */
  std::vector<const tLock*> entries;

  tLockStackData() :
    entries()
  {
    entries.reserve(cMAX_NESTED_LOCKS);
  }
};

static __thread tLockStackData* stack_data = NULL;

static std::string GetLogDescription()
{
  return std::string("Lock stack for thread '") + tThread::CurrentThread().GetName() + "'";
}

/*!
 * Is it okay to call a monitor method with this lock?
 */
bool tLockStack::ConditionVariableLockCorrectlyAcquired(const tLock& lock)
{
  tLockStackData* s = stack_data;
  return s && lock.locked_simple && s->entries.size() > 0 && (*s->entries.rbegin()) == &lock;
}

void tLockStack::DumpStack()
{
  tLockStackData* s = stack_data;
  if (!s)
  {
    RRLIB_LOG_PRINT(eLL_USER, "Current thread has no lock stack (yet).");
    return;
  }
  RRLIB_LOG_PRINT(eLL_USER, "Lock Stack Dump:");
  for (auto it = s->entries.rbegin(); it < s->entries.rend(); it++)
  {
    const tLock* l = *it;
    if (l->locked_ordered)
    {
      RRLIB_LOG_PRINTF(eLL_USER, "  %s %p ('%s', primary %d, secondary %d)", l->locked_simple ? "OrderedMutex" : "RecursiveMutex", l->locked_ordered, l->locked_ordered->GetDescription(), l->locked_ordered->GetPrimary(), l->locked_ordered->GetSecondary());
    }
    else
    {
      RRLIB_LOG_PRINTF(eLL_USER, "  Simple Mutex %p", l->locked_simple);
    }
  }
}

void tLockStack::Push(const tLock* lock)
{
  // make sure that stack exists
  if (!stack_data)
  {
    stack_data = new tLockStackData();
    tThread& t = tThread::CurrentThread();
    // tLock l(*t); no thread-safety issue, since this is always the current thread itself
    t.lock_stack = std::shared_ptr<tLockStackData>(stack_data); // make sure it will be deleted with thread
  }

  tLockStackData* s = stack_data;
  assert(s->entries.size() < (cMAX_NESTED_LOCKS - 1) && "Maximum number of locks exceeded. This likely a programming error or a bad/inefficient implementation.");
  if (s->entries.size() > 0)
  {
    if (!(*s->entries.rbegin())->locked_ordered)
    {
      if (lock->locked_ordered)
      {
        RRLIB_LOG_PRINTF(eLL_ERROR, "Attempt failed to lock ordered mutex %p ('%s', primary %d, secondary %d). You are not allowed to lock another mutex after a simple one.", lock->locked_ordered, lock->locked_ordered->GetDescription(), lock->locked_ordered->GetPrimary(), lock->locked_ordered->GetSecondary());
      }
      else
      {
        RRLIB_LOG_PRINTF(eLL_ERROR, "Attempt failed to lock simple mutex %p. You are not allowed to lock another mutex after a simple one.", lock->locked_simple);
      }
      DumpStack();
      abort();
    }
    else if (lock->locked_ordered && (!lock->locked_ordered->ValidAfter(*(*s->entries.rbegin())->locked_ordered)))
    {
      bool found = false;
      for (auto it = s->entries.begin(); it < s->entries.end(); it++)
      {
        found |= (lock->locked_ordered == (*it)->locked_ordered);
      }
      if (!found)
      {
        RRLIB_LOG_PRINTF(eLL_ERROR, "Attempt failed to lock ordered mutex %p ('%s', primary %d, secondary %d). Lock may not be acquired in this order.", lock->locked_ordered, lock->locked_ordered->GetDescription(), lock->locked_ordered->GetPrimary(), lock->locked_ordered->GetSecondary());
        DumpStack();
        abort();
      }
    }
    else if (lock->locked_ordered && lock->locked_simple)
    {
      bool found = false;
      for (auto it = s->entries.begin(); it < s->entries.end(); it++)
      {
        if (lock->locked_ordered == (*it)->locked_ordered)
        {
          RRLIB_LOG_PRINTF(eLL_ERROR, "Attempt failed to lock ordered mutex %p ('%s', primary %d, secondary %d). Only recursive mutexes may be locked twice.", lock->locked_ordered, lock->locked_ordered->GetDescription(), lock->locked_ordered->GetPrimary(), lock->locked_ordered->GetSecondary());
          DumpStack();
          abort();
        }
      }
    }
  }

  s->entries.push_back(lock);
}

const tLock* tLockStack::Pop()
{
  tLockStackData* s = stack_data;
  assert(s && "No lock stack exists");
  assert(s->entries.size() > 0 && "Lock is not in stack. This should never happen.");
  const tLock* ret = *s->entries.rbegin();
  s->entries.pop_back();
  return ret;
}

//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
#endif
#endif
}
}
}
