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
/*!\file    rrlib/thread/tOrderedMutex.h
 *
 * \author  Max Reichardt
 *
 * \date    2012-07-04
 *
 * \brief   Contains tOrderedMutex
 *
 * \b tOrderedMutex
 *
 * Mutex with info on lock order.
 * After acquiring such a mutex, only simple mutexes, mutexes with a higher lock order and recursive mutexes already locked may may be acquired.
 */
//----------------------------------------------------------------------
#ifndef __rrlib__thread__tOrderedMutex_h__
#define __rrlib__thread__tOrderedMutex_h__

//----------------------------------------------------------------------
// External includes (system with <>, local with "")
//----------------------------------------------------------------------
#include <mutex>

//----------------------------------------------------------------------
// Internal includes with ""
//----------------------------------------------------------------------
#include "rrlib/thread/tMutex.h"
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

//----------------------------------------------------------------------
// Class declaration
//----------------------------------------------------------------------
//! Mutex with info on lock order.
/*!
 * After acquiring such a mutex, only simple mutexes, mutexes with a higher lock order and recursive mutexes already locked may may be acquired.
 */
class tOrderedMutex : public tMutex, public tOrderedMutexBaseClass
{
//----------------------------------------------------------------------
// Public methods and typedefs
//----------------------------------------------------------------------
public:

  explicit tOrderedMutex(const char* description, int primary = 0x7FFFFFFF, int secondary = 0) : // per default, we have an innermost mutex
    tMutex(),
    tOrderedMutexBaseClass(description, primary, secondary)
  {}

//----------------------------------------------------------------------
// Private fields and methods
//----------------------------------------------------------------------
private:

};

//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}


#endif
