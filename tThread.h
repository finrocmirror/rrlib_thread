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
/*!\file    rrlib/thread/tThread.h
 *
 * \author  Max Reichardt
 *
 * \date    2012-07-05
 *
 * \brief   Contains tThread
 *
 * \b tThread
 *
 * Convenient thread class.
 * In some ways similar to Java Threads.
 * Sleep method provides sleeping with respect to "application time".
 */
//----------------------------------------------------------------------
#ifndef __rrlib__thread__tThread_h__
#define __rrlib__thread__tThread_h__

//----------------------------------------------------------------------
// External includes (system with <>, local with "")
//----------------------------------------------------------------------
#include <boost/thread/tss.hpp>
#include <thread>
#include <atomic>
#include "rrlib/logging/messages.h"
#include "rrlib/time/time.h"

//----------------------------------------------------------------------
// Internal includes with ""
//----------------------------------------------------------------------
#include "rrlib/thread/tConditionVariable.h"

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
namespace internal
{

template <typename T>
struct tVectorWithMutex
{
  std::vector<T> vec;
  tOrderedMutex obj_mutex;
  tVectorWithMutex(int lock_order) : vec(), obj_mutex("rrlib_thread tVectorWithMutex", lock_order) {}
};

class tThreadCleanup;
class tThreadDeleter;

} // namespace internal

//----------------------------------------------------------------------
// Class declaration
//----------------------------------------------------------------------
//! Convenient thread class
/*!
 * In some ways similar to Java Threads.
 * Sleep method provides sleeping with respect to "application time".
 */
class tThread : public tMutex
{

//----------------------------------------------------------------------
// Public methods and typedefs
//----------------------------------------------------------------------
public:

  typedef std::thread tWrappedThread;

  /*!
   * Boundaries and default value for priorities
   */
  static const int cMIN_PRIORITY = 1;
  static const int cDEFAULT_PRIORITY = 20;
  static const int cMAX_PRIORITY = 49;

  virtual ~tThread();

  /*!
   * (convenience function)
   *
   * \return Current thread id
   */
  inline static int64_t CurrentThreadId()
  {
    return CurrentThread().GetId();
  }

  /*!
   * \return The thread that is currently executing.
   *
   * (Using the returned reference is only safe as long it is only used by the thread itself.
   *  Otherwise the thread might have already been deleted.
   *  Using CurrentThread().GetSharedPtr() is the safe alternative in this respect.)
   */
  inline static tThread& CurrentThread()
  {
    tThread* result = cur_thread;
    if (result == NULL)   // unknown thread
    {
      result = GetCurThreadLocal().get();
      if (result == NULL)
      {
        result = new tThread(true, false); // will be deleted by thread local
        GetCurThreadLocal().reset(result); // safe because it's the same thread
      }
      cur_thread = result;
    }
    return *result;
  }

  bool GetDeleteOnCompletion() const
  {
    return delete_on_completion;
  }

  /*!
   * \return Returns the rrlib ID of this Thread.
   */
  inline int GetId() const
  {
    return id;
  }

  /*!
   * \return Thread description for logging
   */
  std::string GetLogDescription() const;

  /*!
   * \return Thread description for logging
   */
  static inline const char* GetLogDescriptionStatic()
  {
    return "tThread";
  }

  /*!
   * \return Monitor for thread control and waiting with time stretching support
   */
  inline tConditionVariable& GetMonitor()
  {
    return monitor;
  }

  /*!
   * \return Name of this thread (if not previously set, this is 'Thread-<id>')
   */
  inline std::string GetName() const
  {
    return name;
  }

  /*!
   * \return Native thread handle
   */
  inline std::thread::native_handle_type GetNativeHandle() const
  {
    return handle;
  }

  /*!
   * \return Thread's current priority
   */
  int GetPriority() const
  {
    return priority;
  }

  /*!
   * \return Relevant shared pointer regarding possible auto-deletion of thread
   */
  std::shared_ptr<tThread> GetSharedPtr()
  {
    return self;
  }

  /*!
   * \return Is thread alive (started and not terminated)?
   */
  inline bool IsAlive() const
  {
    return state == tState::RUNNING || state == tState::PREPARE_RUNNING;
  }

  /*!
   * \return Is the stop signal in order to stop this thread set?
   */
  inline bool IsStopSignalSet() const
  {
    return stop_signal;
  }

  /*!
   * Waits for thread to terminate
   */
  void Join();

  /*!
   * \param obj Object (in shared pointer) that thread shall "lock"
   * (it won't be deleted as long as thread exists)
   * (pointers will be reset in reverse order of adding them)
   */
  void LockObject(std::shared_ptr<void> obj);

  /*!
   * Must be implemented by subclass.
   * Called in new thread after it was started.
   * When Run() ends the thread exits.
   */
  virtual void Run()
  {
    RRLIB_LOG_PRINT(WARNING, "No Run method implemented.");
  }

  /*!
   * Setup thread so that it will automatically delete itself
   * when it is finished and there are no further references
   * to it
   */
  void SetAutoDelete()
  {
    delete_on_completion = true;
  }

  /*!
   * \param longevity Determines order in which threads are stopped in StopThreads().
   * Default is zero.
   * Setting this to a higher value stops this thread later than the ones with lower values.
   */
  void SetLongevity(unsigned int longevity)
  {
    this->longevity = longevity;
  }

  /*!
   * (May only be called, before thread is started!)
   *
   * \param name Name for thread
   */
  void SetName(const std::string& name);

  /*!
   * \param new_priority New priority for thread
   */
  void SetPriority(int new_priority)
  {
    // TODO: currently does nothing
    priority = new_priority;
  }

  /*!
   * Makes this thread a real-time thread
   * (sets priority appropriately)
   */
  void SetRealtime();

  /*!
   * The current thread will sleep for the specified amount of time.
   * (Method will call wait() for longer durations => necessary for immediate reaction to time stretching + stopping threads will be quicker)
   *
   * \param wait_for Duration to wait
   * \param use_application_time Is duration specified in "application time" instead of system time? (see rrlib/time/time.h).
   * \param wait_until Time point until to wait (optional). If specified, this function won't need call clock's now() in case application time is used.
   */
  static void Sleep(const rrlib::time::tDuration& sleep_for, bool use_application_time, rrlib::time::tTimestamp wait_until = rrlib::time::cNO_TIME);

  /*!
   * Starts thread.
   *
   * Note, that a terminated thread cannot be restarted.
   */
  void Start();

  /*!
   * Tries to stop thread - but not by force
   *
   * Should be overridden by subclasses to provide proper implementations
   */
  virtual void StopThread();

  /*!
   * Tries to stop all known threads
   */
  static void StopThreads()
  {
    StopThreads(false);
  }

  /*!
   * Returns whether an attempt was already made to stop all threads
   */
  static bool StoppingThreads()
  {
    return StopThreads(true);
  }

  /*!
   * Hint to the scheduler that current thread is willing to yield.
   */
  static void Yield();

//----------------------------------------------------------------------
// Protected fields and methods
//----------------------------------------------------------------------
protected:

  /*!
   * Default constructor for derived classes.
   *
   * \param name Name of thread (optional)
   */
  tThread(const std::string& name = "");

  /*!
   * \param value New value for signal for stopping thread
   */
  void SetStopSignal(bool value)
  {
    stop_signal = value;
  }

//----------------------------------------------------------------------
// Private fields and methods
//----------------------------------------------------------------------
private:

  friend class internal::tLockStack;
  friend class internal::tThreadCleanup;

  /*!
   * Thread state
   */
  enum class tState
  {
    NEW,             //!< Thread has not started yet
    PREPARE_RUNNING, //!< Preparing to run thread
    RUNNING,         //!< Thread is running
    TERMINATED       //!< Thread has terminated
  };

  /*! ID that indicates that there's no valid thread id in thread local - needs to be different for windows */
  static const int cNO_THREAD_ID = -1;

  /*! Signal for stopping thread */
  std::atomic<bool> stop_signal;

  /*! Holds on to lock stack as long as thread exists */
  std::shared_ptr<void> lock_stack;

  /*! Id of Thread - generated by this class */
  const int id;

  /*! Name of Thread */
  std::string name;

  /*! Thread priority */
  int priority;

  /*! Current Thread state */
  tState state;

  /*! manager pointer to object itself */
  std::shared_ptr<tThread> self;

  /*! delete runnable/self when thread is completed and no more derived shared pointers point to object */
  bool delete_on_completion;

  /*! Signal for starting thread */
  bool start_signal;

  /*! Monitor for thread control and waiting with time stretching support */
  tConditionVariable monitor;

  /*! Threads own reference to threadList */
  std::shared_ptr<internal::tVectorWithMutex<std::weak_ptr<tThread>>> thread_list_ref;

  /*! Objects (shared pointers) that thread has locked (won't be deleted as long as thread exists) */
  std::vector<std::shared_ptr<void>> locked_objects;

  /*! Reference to current thread */
  static __thread tThread* cur_thread;

  /*!
   * Determines order in which threads are stopped in StopThreads().
   * Default is zero.
   * Setting this to a higher value stops this thread later than the ones with lower values.
   */
  unsigned int longevity;

  /*! True, if this is a thread that was not created via this class */
  const bool unknown_thread;

  /*! wrapped thread */
  tWrappedThread wrapped_thread;

  /*! pthread/whatever handle */
  std::thread::native_handle_type handle;

  /*! Number of threads that are joining */
  std::atomic<int> joining_threads;


  /*!
   * Create object that handles threads not created via this class
   * (we only have the two parameters to avoid ambiguities with the ordinary constructor - e.g. when passing a const char*)
   */
  tThread(bool anonymous, bool legion);

  /*!
   * Add thread to thread list
   */
  void AddToThreadList();

  /*! Stores thread local information on current thread */
  static boost::thread_specific_ptr<tThread>& GetCurThreadLocal();

  /*!
   * Newly created threads enter this method
   */
  static void Launch(tThread* thread_ptr);

  /*!
   * Called by method above
   */
  void Launcher();

  /*!
   * Helper method for joining
   */
  void PreJoin();

  /*!
   * Implementation of above methods - It's important to have the variable in the code to keep during static destruction
   * (That's why we have this 2-in-1 implementation)
   *
   * Tries to stop all known threads
   *
   * \param query_only Query only whether threads have already been deleted?
   * \return Returns whether an attempt was (already) made to stop all threads
   */
  static bool StopThreads(bool query_only);
};

//----------------------------------------------------------------------
// End of namespace declaration
//----------------------------------------------------------------------
}
}


#endif
