/**
 * @file   llcond.h
 * @author Nat Goodspeed
 * @date   2019-07-10
 * @brief  LLCond is a wrapper around condition_variable to encapsulate the
 *         obligatory condition_variable usage pattern. We also provide
 *         simplified versions LLScalarCond, LLBoolCond and LLOneShotCond.
 * 
 * $LicenseInfo:firstyear=2019&license=viewerlgpl$
 * Copyright (c) 2019, Linden Research, Inc.
 * $/LicenseInfo$
 */

#if ! defined(LL_LLCOND_H)
#define LL_LLCOND_H

#include <boost/fiber/condition_variable.hpp>
#include <mutex>
#include <chrono>

/**
 * LLCond encapsulates the pattern required to use a condition_variable. It
 * bundles subject data, a mutex and a condition_variable: the three required
 * data objects. It provides wait() methods analogous to condition_variable,
 * but using the contained condition_variable and the contained mutex. It
 * provides modify() methods accepting an invocable to safely modify the
 * contained data and notify waiters. These methods implicitly perform the
 * required locking.
 *
 * The generic LLCond template assumes that DATA might be a struct or class.
 * For a scalar DATA type, consider LLScalarCond instead. For specifically
 * bool, consider LLBoolCond.
 *
 * Use of boost::fibers::condition_variable makes LLCond work between
 * coroutines as well as between threads.
 */
template <typename DATA>
class LLCond
{
private:
    // This is the DATA controlled by the condition_variable.
    DATA mData;
    // condition_variable must be used in conjunction with a mutex. Use
    // boost::fibers::mutex instead of std::mutex because the latter blocks
    // the entire calling thread, whereas the former blocks only the current
    // coroutine within the calling thread. Yet boost::fiber::mutex is safe to
    // use across threads as well: it subsumes std::mutex functionality.
    boost::fibers::mutex mMutex;
    // Use boost::fibers::condition_variable for the same reason.
    boost::fibers::condition_variable mCond;

public:
    /// LLCond can be explicitly initialized with a specific value for mData if
    /// desired.
    LLCond(DATA&& init=DATA()):
        mData(init)
    {}

    /// LLCond is move-only
    LLCond(const LLCond&) = delete;
    LLCond& operator=(const LLCond&) = delete;

    /// get() returns a const reference to the stored DATA. The only way to
    /// get a non-const reference -- to modify the stored DATA -- is via
    /// update_one() or update_all().
    const DATA& get() const { return mData; }

    /**
     * Pass update_one() an invocable accepting non-const (DATA&). The
     * invocable will presumably modify the referenced DATA. update_one()
     * will lock the mutex, call the invocable and then call notify_one() on
     * the condition_variable.
     *
     * For scalar DATA, it's simpler to use LLScalarCond::set_one(). Use
     * update_one() when DATA is a struct or class.
     */
    template <typename MODIFY>
    void update_one(MODIFY modify)
    {
        { // scope of lock can/should end before notify_one()
            std::unique_lock<boost::fibers::mutex> lk(mMutex);
            modify(mData);
        }
        mCond.notify_one();
    }

    /**
     * Pass update_all() an invocable accepting non-const (DATA&). The
     * invocable will presumably modify the referenced DATA. update_all()
     * will lock the mutex, call the invocable and then call notify_all() on
     * the condition_variable.
     *
     * For scalar DATA, it's simpler to use LLScalarCond::set_all(). Use
     * update_all() when DATA is a struct or class.
     */
    template <typename MODIFY>
    void update_all(MODIFY modify)
    {
        { // scope of lock can/should end before notify_all()
            std::unique_lock<boost::fibers::mutex> lk(mMutex);
            modify(mData);
        }
        mCond.notify_all();
    }

    /**
     * Pass wait() a predicate accepting (const DATA&), returning bool. The
     * predicate returns true when the condition for which it is waiting has
     * been satisfied, presumably determined by examining the referenced DATA.
     * wait() locks the mutex and, until the predicate returns true, calls
     * wait() on the condition_variable.
     */
    template <typename Pred>
    void wait(Pred pred)
    {
        std::unique_lock<boost::fibers::mutex> lk(mMutex);
        // We must iterate explicitly since the predicate accepted by
        // condition_variable::wait() requires a different signature:
        // condition_variable::wait() calls its predicate with no arguments.
        // Fortunately, the loop is straightforward.
        // We advise the caller to pass a predicate accepting (const DATA&).
        // But what if they instead pass a predicate accepting non-const
        // (DATA&)? Such a predicate could modify mData, which would be Bad.
        // Forbid that.
        while (! pred(const_cast<const DATA&>(mData)))
        {
            mCond.wait(lk);
        }
    }

    /**
     * Pass wait_until() a chrono::time_point, indicating the time at which we
     * should stop waiting, and a predicate accepting (const DATA&), returning
     * bool. The predicate returns true when the condition for which it is
     * waiting has been satisfied, presumably determined by examining the
     * referenced DATA. wait_until() locks the mutex and, until the predicate
     * returns true, calls wait_until() on the condition_variable.
     * wait_until() returns false if condition_variable::wait_until() timed
     * out without the predicate returning true.
     */
    template <typename Clock, typename Duration, typename Pred>
    bool wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time, Pred pred)
    {
        std::unique_lock<boost::fibers::mutex> lk(mMutex);
        // see wait() for comments about this const_cast
        while (! pred(const_cast<const DATA&>(mData)))
        {
            if (boost::fibers::cv_status::timeout == mCond.wait_until(lk, timeout_time))
            {
                // It's possible that wait_until() timed out AND the predicate
                // became true more or less simultaneously. Even though
                // wait_until() timed out, check the predicate one more time.
                return pred(const_cast<const DATA&>(mData));
            }
        }
        return true;
    }

    /**
     * Pass wait_for() a chrono::duration, indicating how long we're willing
     * to wait, and a predicate accepting (const DATA&), returning bool. The
     * predicate returns true when the condition for which it is waiting has
     * been satisfied, presumably determined by examining the referenced DATA.
     * wait_for() locks the mutex and, until the predicate returns true, calls
     * wait_for() on the condition_variable. wait_for() returns false if
     * condition_variable::wait_for() timed out without the predicate
     * returning true.
     */
    template <typename Rep, typename Period, typename Pred>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout_duration, Pred pred)
    {
        // Instead of replicating wait_until() logic, convert duration to
        // time_point and just call wait_until().
        // An implementation in which we repeatedly called
        // condition_variable::wait_for() with our passed duration would be
        // wrong! We'd keep pushing the timeout time farther and farther into
        // the future. This way, we establish a definite timeout time and
        // stick to it.
        return wait_until(std::chrono::steady_clock::now() + timeout_duration, pred);
    }
};

template <typename DATA>
class LLScalarCond: public LLCond<DATA>
{
    using super = LLCond<DATA>;

public:
    /// LLScalarCond can be explicitly initialized with a specific value for
    /// mData if desired.
    LLCond(DATA&& init=DATA()):
        super(init)
    {}

    /// Pass set_one() a new value to which to update mData. set_one() will
    /// lock the mutex, update mData and then call notify_one() on the
    /// condition_variable.
    void set_one(DATA&& value)
    {
        super::update_one([](DATA& data){ data = value; });
    }

    /// Pass set_all() a new value to which to update mData. set_all() will
    /// lock the mutex, update mData and then call notify_all() on the
    /// condition_variable.
    void set_all(DATA&& value)
    {
        super::update_all([](DATA& data){ data = value; });
    }

    /**
     * Pass wait_equal() a value for which to wait. wait_equal() locks the
     * mutex and, until the stored DATA equals that value, calls wait() on the
     * condition_variable.
     */
    void wait_equal(const DATA& value)
    {
        super::wait([&value](const DATA& data){ return (data == value); });
    }

    /**
     * Pass wait_until_equal() a chrono::time_point, indicating the time at
     * which we should stop waiting, and a value for which to wait.
     * wait_until_equal() locks the mutex and, until the stored DATA equals
     * that value, calls wait_until() on the condition_variable.
     * wait_until_equal() returns false if condition_variable::wait_until()
     * timed out without the stored DATA being equal to the passed value.
     */
    template <typename Clock, typename Duration>
    bool wait_until_equal(const std::chrono::time_point<Clock, Duration>& timeout_time,
                          const DATA& value)
    {
        return super::wait_until(timeout_time,
                                 [&value](const DATA& data){ return (data == value); });
    }

    /**
     * Pass wait_for_equal() a chrono::duration, indicating how long we're
     * willing to wait, and a value for which to wait. wait_for_equal() locks
     * the mutex and, until the stored DATA equals that value, calls
     * wait_for() on the condition_variable. wait_for_equal() returns false if
     * condition_variable::wait_for() timed out without the stored DATA being
     * equal to the passed value.
     */
    template <typename Rep, typename Period>
    bool wait_for_equal(const std::chrono::duration<Rep, Period>& timeout_duration,
                        const DATA& value)
    {
        return super::wait_for(timeout_duration,
                               [&value](const DATA& data){ return (data == value); });
    }

    /**
     * Pass wait_unequal() a value from which to move away. wait_unequal()
     * locks the mutex and, until the stored DATA no longer equals that value,
     * calls wait() on the condition_variable.
     */
    void wait_unequal(const DATA& value)
    {
        super::wait([&value](const DATA& data){ return (data != value); });
    }

    /**
     * Pass wait_until_unequal() a chrono::time_point, indicating the time at
     * which we should stop waiting, and a value from which to move away.
     * wait_until_unequal() locks the mutex and, until the stored DATA no
     * longer equals that value, calls wait_until() on the condition_variable.
     * wait_until_unequal() returns false if condition_variable::wait_until()
     * timed out with the stored DATA still being equal to the passed value.
     */
    template <typename Clock, typename Duration>
    bool wait_until_unequal(const std::chrono::time_point<Clock, Duration>& timeout_time,
                          const DATA& value)
    {
        return super::wait_until(timeout_time,
                                 [&value](const DATA& data){ return (data != value); });
    }

    /**
     * Pass wait_for_unequal() a chrono::duration, indicating how long we're
     * willing to wait, and a value from which to move away.
     * wait_for_unequal() locks the mutex and, until the stored DATA no longer
     * equals that value, calls wait_for() on the condition_variable.
     * wait_for_unequal() returns false if condition_variable::wait_for()
     * timed out with the stored DATA still being equal to the passed value.
     */
    template <typename Rep, typename Period>
    bool wait_for_unequal(const std::chrono::duration<Rep, Period>& timeout_duration,
                        const DATA& value)
    {
        return super::wait_for(timeout_duration,
                               [&value](const DATA& data){ return (data != value); });
    }
};

/// Using bool as LLScalarCond's DATA seems like a particularly useful case
using LLBoolCond = LLScalarCond<bool>;

// LLOneShotCond -- init false, set (and wait for) true? Or full suite?
class LLOneShotCond: public LLBoolCond
{
    using super = LLBoolCond;

public:
    /// The bool stored in LLOneShotCond is initially false
    LLOneShotCond(): super(false) {}

    /// LLOneShotCond assumes that nullary set_one() means to set its bool true
    void set_one(bool value=true)
    {
        super::set_one(value);
    }

    /// LLOneShotCond assumes that nullary set_all() means to set its bool true
    void set_all(bool value=true)
    {
        super::set_all(value);
    }

    /**
     * wait() locks the mutex and, until the stored bool is true, calls wait()
     * on the condition_variable.
     */
    void wait()
    {
        super::wait_equal(true);
    }

    /**
     * Pass wait_until() a chrono::time_point, indicating the time at which we
     * should stop waiting. wait_until() locks the mutex and, until the stored
     * bool is true, calls wait_until() on the condition_variable.
     * wait_until() returns false if condition_variable::wait_until() timed
     * out without the stored bool being true.
     */
    template <typename Clock, typename Duration>
    bool wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time)
    {
        return super::wait_until_equal(timeout_time, true);
    }

    /**
     * Pass wait_for() a chrono::duration, indicating how long we're willing
     * to wait. wait_for() locks the mutex and, until the stored bool is true,
     * calls wait_for() on the condition_variable. wait_for() returns false if
     * condition_variable::wait_for() timed out without the stored bool being
     * true.
     */
    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout_duration)
    {
        return super::wait_for_equal(timeout_duration, true);
    }
};

#endif /* ! defined(LL_LLCOND_H) */