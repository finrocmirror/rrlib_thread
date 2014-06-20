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
/*!\file    rrlib/thread/tOrderedMutexBaseClass.h
 *
 * \author  Max Reichardt
 *
 * \date    2012-07-04
 *
 * \brief   Contains tOrderedMutexBaseClass
 *
 * \b tOrderedMutexBaseClass
 *
 * Base class for ordered mutexes.
 * Contains lock order and description.
 */
//----------------------------------------------------------------------
#ifndef __rrlib__thread__tOrderedMutexBaseClass_h__
#define __rrlib__thread__tOrderedMutexBaseClass_h__

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

//----------------------------------------------------------------------
// Class declaration
//----------------------------------------------------------------------
//! Base class for ordered mutexes.
/*!
 * Contains lock order and description.
 */
class tOrderedMutexBaseClass
{
//----------------------------------------------------------------------
// Public methods and typedefs
//----------------------------------------------------------------------
public:

  explicit tOrderedMutexBaseClass(const char* description, int primary, int secondary) :
#ifndef RRLIB_SINGLE_THREADED
    description(description),
#endif
    primary(primary),
    secondary(secondary)
  {}

  /*!
   * \return Lock description - for lock stack debug output
   */
  const char* GetDescription() const
  {
#ifndef RRLIB_SINGLE_THREADED
    return description;
#else
    return "";
#endif
  }

  /*!
   * \return Primary lock order (higher is later)
   */
  int GetPrimary() const
  {
    return primary;
  }

  /*!
   * \return Secondary lock order (higher is later)
   */
  int GetSecondary() const
  {
    return secondary;
  }

  /*!
   * \return Can this mutex be acquired after the other one?
   */
  bool ValidAfter(const tOrderedMutexBaseClass& other) const
  {
    if (&other == this)
    {
      return true;
    }
    if (primary < other.primary)
    {
      return false;
    }
    else if (primary > other.primary)
    {
      return true;
    }
    else
    {
      if (secondary == other.secondary)
      {
        assert(false && "Equal lock orders are not allowed");
      }
      return secondary > other.secondary;
    }
  }

//----------------------------------------------------------------------
// Private fields and methods
//----------------------------------------------------------------------
private:

  /*! Lock description - for lock stack debug output */
#ifndef RRLIB_SINGLE_THREADED
  const char* const description;
#endif

  /*! Primary and secondary lock order (larger is later) */
  const int primary, secondary;
};

//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}


#endif
