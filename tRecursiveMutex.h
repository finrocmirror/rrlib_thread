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
/*!\file    rrlib/thread/tRecursiveMutex.h
 *
 * \author  Max Reichardt
 *
 * \date    2012-07-04
 *
 * \brief   Contains tRecursiveMutex
 *
 * \b tRecursiveMutex
 *
 * Recursive Mutex with info on lock order.
 * Note, that some comments suggest that using recursive mutexes indicates suboptimal design.
 */
//----------------------------------------------------------------------
#ifndef __rrlib__thread__tRecursiveMutex_h__
#define __rrlib__thread__tRecursiveMutex_h__

//----------------------------------------------------------------------
// External includes (system with <>, local with "")
//----------------------------------------------------------------------
#include <mutex>

//----------------------------------------------------------------------
// Internal includes with ""
//----------------------------------------------------------------------
#include "rrlib/thread/tOrderedMutexBaseClass.h"

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

//----------------------------------------------------------------------
// Class declaration
//----------------------------------------------------------------------
//! Recursive Mutex with info on lock order.
/*!
 * After acquiring such a mutex, only simple mutexes, mutexes with a higher lock order and recursive mutexes already locked may may be acquired.
 * Note, that some comments suggest that using recursive mutexes indicates suboptimal design.
 */
class tRecursiveMutex : public tOrderedMutexBaseClass
{
  friend class tLock;

//----------------------------------------------------------------------
// Public methods and typedefs
//----------------------------------------------------------------------
public:

  typedef rrlib::thread::tLock tLock;

  explicit tRecursiveMutex(const char* description, int primary = 0x7FFFFFFF, int secondary = 0) : // per default, we have an innermost mutex
    tOrderedMutexBaseClass(description, primary, secondary)
#ifndef RRLIB_SINGLE_THREADED
    , wrapped()
#endif
  {}

//----------------------------------------------------------------------
// Private fields and methods
//----------------------------------------------------------------------
private:

#ifndef RRLIB_SINGLE_THREADED

  /*! Wrapped mutex class */
  mutable std::recursive_mutex wrapped;

#endif
};

//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}


#endif
