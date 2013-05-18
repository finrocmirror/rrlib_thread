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
/*!\file    rrlib/thread/tNoMutex.h
 *
 * \author  Max Reichardt
 *
 * \date    2012-07-04
 *
 * \brief   Contains tNoMutex
 *
 * \b tNoMutex
 *
 * Noop mutex. Can be useful as template parameter for certain classes to disable any internal locking.
 *
 */
//----------------------------------------------------------------------
#ifndef __rrlib__thread__tNoMutex_h__
#define __rrlib__thread__tNoMutex_h__

//----------------------------------------------------------------------
// External includes (system with <>, local with "")
//----------------------------------------------------------------------
#include "rrlib/util/tNoncopyable.h"

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

//----------------------------------------------------------------------
// Class declaration
//----------------------------------------------------------------------
//! Noop mutex
/*!
 * Noop mutex. Can be useful as template parameter for certain classes to disable any internal locking.
 */
class tNoMutex : private util::tNoncopyable
{
//----------------------------------------------------------------------
// Public methods and typedefs
//----------------------------------------------------------------------
public:

  typedef rrlib::thread::tLock tLock;

  explicit tNoMutex(const char* description = "", int primary = 0x7FFFFFFF, int secondary = 0) {}

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
