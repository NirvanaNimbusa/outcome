/* future.hpp
Non-allocating constexpr future-promise
(C) 2015 Niall Douglas http://www.nedprod.com/
File Created: May 2015


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef BOOST_SPINLOCK_FUTURE_HPP
#define BOOST_SPINLOCK_FUTURE_HPP

#include "spinlock.hpp"
#include <future>

BOOST_SPINLOCK_V1_NAMESPACE_BEGIN
namespace lightweight_futures {

template<typename R> class future;
template<typename R> class promise;
using std::exception_ptr;
using std::make_exception_ptr;
using std::error_code;
using std::rethrow_exception;
using std::future_error;
using std::future_errc;
using std::system_error;

namespace detail
{
  template<typename R> struct value_storage
  {
#if 1 //ndef _MSC_VER
    union
    {
      R value;
      error_code error;
      exception_ptr exception;
      future<R> *future_;
    };
#else
    // Sadly unrestricted unions here did not work well on MSVC, so do it by hand.
    char _buffer[sizeof(R)>sizeof(error_code) ? sizeof(R) : sizeof(error_code)];
    
    R &value() { return *(R *)_buffer; }
    exception_ptr &exception() { static_assert(sizeof(exception_ptr)<=sizeof(_buffer), "exception_ptr too big"); return *(exception_ptr *)_buffer; }
    error_code &error() { return *(error_code *)_buffer; }
    future<R> *&future_() { return *(future<R> **)_buffer; }
#define value value()
#define exception exception()
#define error error()
#define future_ future_()
#endif
    
    enum class storage_type
    {
      empty,
      value,
      error,
      exception,
      future
    } type;
    
    BOOST_CONSTEXPR value_storage() : type(storage_type::empty)
    {
    }
    BOOST_CONSTEXPR value_storage(const R &v) : value(v), type(storage_type::value) { }
    BOOST_CONSTEXPR value_storage(const error_code &v) : error(v), type(storage_type::error) { }
    BOOST_CONSTEXPR value_storage(const exception_ptr &v) : exception(v), type(storage_type::exception) { }
    BOOST_CONSTEXPR value_storage(R &&v) : value(std::move(v)), type(storage_type::value) { }
    BOOST_CONSTEXPR value_storage(error_code &&v) : error(std::move(v)), type(storage_type::error) { }
    BOOST_CONSTEXPR value_storage(exception_ptr &&v) : exception(std::move(v)), type(storage_type::exception) { }
    BOOST_CXX14_CONSTEXPR value_storage(value_storage &&o) noexcept(std::is_nothrow_move_constructible<R>::value && std::is_nothrow_move_constructible<exception_ptr>::value && std::is_nothrow_move_constructible<error_code>::value) : type(o.type)
    {
      switch(type)
      {
        case storage_type::empty:
          break;
        case storage_type::value:
          new (&value) R(std::move(o.value));
          break;
        case storage_type::error:
          new (&error) error_code(std::move(o.error));
          break;
        case storage_type::exception:
          new (&exception) exception_ptr(std::move(o.exception));
          break;
        case storage_type::future:
          future_=o.future_;
          o.future_=nullptr;
          break;
      }      
      o.type=storage_type::empty;
    }
    BOOST_CXX14_CONSTEXPR value_storage &operator=(value_storage &&o) noexcept(std::is_nothrow_move_constructible<R>::value && std::is_nothrow_move_constructible<exception_ptr>::value && std::is_nothrow_move_constructible<error_code>::value)
    {
      // TODO FIXME: Only safe if both of these are noexcept
      this->~value_storage();
      new (this) value_storage(std::move(o));
      return *this;
    }
    ~value_storage() noexcept(std::is_nothrow_destructible<R>::value && std::is_nothrow_destructible<exception_ptr>::value && std::is_nothrow_destructible<error_code>::value) { reset(); }
    BOOST_CXX14_CONSTEXPR void swap(storage_type &o) noexcept(std::is_nothrow_move_constructible<R>::value && std::is_nothrow_move_constructible<exception_ptr>::value && std::is_nothrow_move_constructible<error_code>::value)
    {
      switch(type)
      {
        case storage_type::empty:
          break;
        case storage_type::value:
          std::swap(value, o.value);
          break;
        case storage_type::error:
          std::swap(error, o.error);
          break;
        case storage_type::exception:
          std::swap(exception, o.exception);
          break;
        case storage_type::future:
          std::swap(future_, o.future_);
          break;
      }      
    }
    BOOST_CXX14_CONSTEXPR void reset() noexcept(std::is_nothrow_destructible<R>::value && std::is_nothrow_destructible<exception_ptr>::value && std::is_nothrow_destructible<error_code>::value)
    {
      switch(type)
      {
        case storage_type::empty:
          break;
        case storage_type::value:
          value.~R();
          type=storage_type::empty;
          break;
        case storage_type::error:
          error.~error_code();
          type=storage_type::empty;
          break;
        case storage_type::exception:
          exception.~exception_ptr();
          type=storage_type::empty;
          break;
        case storage_type::future:
          future_=nullptr;
          type=storage_type::empty;
          break;
      }
    }
    template<class U> BOOST_CXX14_CONSTEXPR void set_value(U &&v)
    {
      if(type!=storage_type::empty)
        throw future_error(future_errc::promise_already_satisfied);
      new (&value) R(std::forward<U>(v));
      type=storage_type::value;
    }
    void set_exception(exception_ptr e)
    {
      if(type!=storage_type::empty)
        throw future_error(future_errc::promise_already_satisfied);
      new (&exception) exception_ptr(std::move(e));
      type=storage_type::exception;
    }
    void set_error(error_code e)
    {
      if(type!=storage_type::empty)
        throw future_error(future_errc::promise_already_satisfied);
      new (&error) error_code(std::move(e));
      type=storage_type::error;
    }
    // Called by future to take ownership of storage from promise
    BOOST_CXX14_CONSTEXPR void set_future(future<R> *f)
    {
      // Always overwrites existing storage
      reset();
      future_=f;
      type=storage_type::future;
    }
  };
}

/*! \brief class monad
\brief Implements a monadic value transport
*/
template<typename R, bool _is_consuming> class monad
{
public:
  typedef R value_type;
  typedef error_code error_type;
  typedef exception_ptr exception_type;
  BOOST_STATIC_CONSTEXPR bool is_consuming=_is_consuming;
private:
  typedef detail::value_storage<value_type> value_storage_type;
  value_storage_type _storage;
protected:
  monad(value_storage_type &&s) : _storage(std::move(s)) { }
public:
  monad() = default;
  BOOST_CONSTEXPR monad(const value_type &v) : _storage(v) { }
  BOOST_CONSTEXPR monad(value_type &&v) : _storage(std::move(v)) { }
  BOOST_CONSTEXPR monad(const error_type &v) : _storage(v) { }
  BOOST_CONSTEXPR monad(error_type &&v) : _storage(std::move(v)) { }
  BOOST_CONSTEXPR monad(const exception_type &v) : _storage(v) { }
  BOOST_CONSTEXPR monad(exception_type &&v) : _storage(std::move(v)) { }
  monad(monad &&) = default;
  monad &operator=(monad &&) = default;
  monad(const monad &)=default;
  monad &operator=(const monad &)=default;
  
  BOOST_CONSTEXPR bool is_ready() const noexcept
  {
    return _storage.type!=value_storage_type::storage_type::empty;
  }
  BOOST_CONSTEXPR bool has_exception() const noexcept
  {
    return _storage.type==value_storage_type::storage_type::exception || _storage.type==value_storage_type::storage_type::error;
  }
  BOOST_CONSTEXPR bool has_error() const noexcept
  {
    return _storage.type==value_storage_type::storage_type::error;
  }
  BOOST_CONSTEXPR bool has_value() const noexcept
  {
    return _storage.type==value_storage_type::storage_type::value;
  }

  void swap(monad &o) noexcept(std::is_nothrow_move_constructible<value_storage_type>::value)
  {
    _storage.swap(o._storage);
  }
  void reset() noexcept(std::is_nothrow_destructible<value_storage_type>::value)
  {
    _storage.reset();
  }
  
  value_type get()
  {
    if(!is_ready())
      throw future_error(future_errc::no_state);
    if(has_error() || has_exception())
    {
      exception_ptr e;
      if(has_error())
        e=make_exception_ptr(system_error(_storage.error));
      if(has_exception())
        e=_storage.exception;
      if(is_consuming)
        _storage.reset();
      rethrow_exception(e);
    }
    if(is_consuming)
    {
      value_type ret(std::move(_storage.value));
      _storage.reset();
      return std::move(ret);
    }
    else
    {
      value_type ret(_storage.value);
      return std::move(ret);
    }
  }
  // value_type get_or(const value_type &);  // TODO
  // value_type get_or(value_type &&);  // TODO
  void set_value(const value_type &v) { _storage.reset(); _storage.set_value(v); }
  void set_value(value_type &&v) { _storage.reset(); _storage.set_value(v); }
  error_type get_error()
  {
    if(!is_ready())
      throw future_error(future_errc::no_state);
    if(!has_error())
      return error_type();
    error_type e(std::move(_storage.error));
    if(is_consuming)
      _storage.reset();
    return e;
  }
  void set_error(const error_type &v) { _storage.reset(); _storage.set_error(v); }
  void set_error(error_type &&v) { _storage.reset(); _storage.set_error(v); }
  exception_type get_exception()
  {
    if(!is_ready())
      throw future_error(future_errc::no_state);
    if(!has_error() && !has_exception())
      return exception_type();
    exception_type e;
    if(has_error())
      e=make_exception_ptr(system_error(_storage.error));
    if(has_exception())
      e=_storage.exception;
    if(is_consuming)
      _storage.reset();
    return e;
  }
  void set_exception(const exception_type &v) { _storage.reset(); _storage.set_exception(v); }
  void set_exception(exception_type &&v) { _storage.reset(); _storage.set_exception(v); }
  template<typename E> void set_exception(E &&e)
  {
    set_exception(make_exception_ptr(std::forward<E>(e)));
  }
      
  // TODO Where F would return a future<future<...>>, we unwrap to a single future<R>
  // template<class F> typename std::result_of<F(future)>::type then(F &&f);
};


namespace detail
{
  template<typename R> struct lock_guard
  {
    promise<R> *_p;
    future<R>  *_f;
    lock_guard(const lock_guard &)=delete;
    lock_guard(lock_guard &&)=delete;
    lock_guard(promise<R> *p) : _p(nullptr), _f(nullptr)
    {
      // constexpr fold
      if(!p->_need_locks)
      {
        _p=p;
        if(p->_storage.type==value_storage<R>::storage_type::future)
          _f=p->_storage.future_;
        return;
      }
      else for(;;)
      {
        p->_lock().lock();
        if(p->_storage.type==value_storage<R>::storage_type::future)
        {
          if(p->_storage.future_->_lock().try_lock())
          {
            _p=p;
            _f=p->_storage.future_;
            break;
          }
        }
        else
        {
          _p=p;
          break;
        }
        p->_lock().unlock();
      }
    }
    lock_guard(future<R> *f) : _p(nullptr), _f(nullptr)
    {
      // constexpr fold
      if(!f->_need_locks)
      {
        _p=f->_promise;
        _f=f;
        return;
      }
      else for(;;)
      {
        f->_lock().lock();
        if(f->_promise)
        {
          if(f->_promise->_lock().try_lock())
          {
            _p=f->_promise;
            _f=f;
            break;
          }
        }
        else
        {
          _f=f;
          break;
        }
        f->_lock().unlock();
      }
    }
    ~lock_guard()
    {
      unlock();
    }
    void unlock()
    {
      if(_p)
      {
        if(_p->_need_locks)
          _p->_lock().unlock();
        _p=nullptr;
      }
      if(_f)
      {
        if(_f->_need_locks)
          _f->_lock().unlock();
        _f=nullptr;
      }
    }
  };
}

template<typename R> class promise
{
  friend class future<R>;
  friend struct detail::lock_guard<R>;
public:
  typedef R value_type;
  typedef exception_ptr exception_type;
  typedef error_code error_type;
private:
  typedef detail::value_storage<value_type> value_storage_type;
  value_storage_type _storage;
  bool _need_locks;  // Used to inhibit unnecessary atomic use, thus enabling constexpr collapse
  char _lock_buffer[sizeof(spinlock<bool>)];
  spinlock<bool> &_lock() { return *(spinlock<bool> *)_lock_buffer; }
public:
  //! \brief EXTENSION: constexpr capable constructor
  BOOST_CONSTEXPR promise() : _need_locks(false)
  {
    static_assert(std::is_move_constructible<value_type>::value, "Type must be move constructible to be used in a lightweight promise");    
  }
  // template<class Allocator> promise(allocator_arg_t, Allocator a); // cannot support
  BOOST_CXX14_CONSTEXPR promise(promise &&o) noexcept(std::is_nothrow_move_constructible<value_storage_type>::value) : _need_locks(o._need_locks)
  {
    if(_need_locks) new (&_lock()) spinlock<bool>();
    detail::lock_guard<value_type> h(&o);
    _storage=std::move(o._storage);
    if(h._f)
      h._f->_promise=this;
  }
  promise &operator=(promise &&o) noexcept(std::is_nothrow_move_constructible<value_storage_type>::value)
  {
    // TODO FIXME: Only safe if both of these are noexcept
    this->~promise();
    new (this) promise(std::move(o));
    return *this;
  }
  promise(const promise &)=delete;
  promise &operator=(const promise &)=delete;
  ~promise() noexcept(std::is_nothrow_destructible<value_storage_type>::value)
  {
    detail::lock_guard<value_type> h(this);
    if(h._f)
    {
      if(!h._f->is_ready())
        h._f->set_exception(make_exception_ptr(future_error(future_errc::broken_promise)));
      h._f->_promise=nullptr;
    }
    // Destroy myself before locks exit
    _storage.reset();
  }
  
  void swap(promise &o) noexcept(std::is_nothrow_move_constructible<value_storage_type>::value)
  {
    detail::lock_guard<value_type> h1(this), h2(&o);
    _storage.swap(o._storage);
    if(h1._f)
      h1._f->_promise=&o;
    if(h2._f)
      h2._f->_promise=this;
  }
  
  future<value_type> get_future()
  {
    // If no value stored yet, I need locks on from now on
    if(!_need_locks && _storage.type==value_storage_type::storage_type::empty)
    {
      _need_locks=true;
      new (&_lock()) spinlock<bool>();
    }
    detail::lock_guard<value_type> h(this);
    if(h._f)
      throw future_error(future_errc::future_already_retrieved);
    future<value_type> ret(this);
    h.unlock();
    return std::move(ret);
  }
  //! \brief EXTENSION: Does this promise have a future?
  bool has_future() const noexcept
  {
    //detail::lock_guard<value_type> h(this);
    return _storage.type==value_storage_type::storage_type::future;
  }
  
  void set_value(const value_type &v) noexcept(std::is_nothrow_copy_constructible<value_type>::value)
  {
    detail::lock_guard<value_type> h(this);
    if(h._f)
      h._f->set_value(v);
    else
      _storage.set_value(v);
  }
  void set_value(value_type &&v) noexcept(std::is_nothrow_move_constructible<value_type>::value)
  {
    detail::lock_guard<value_type> h(this);
    if(h._f)
      h._f->set_value(std::move(v));
    else
      _storage.set_value(std::move(v));
  }
  //! \brief EXTENSION: Set an error code (doesn't allocate)
  void set_error(error_type e) noexcept(std::is_nothrow_copy_constructible<error_type>::value)
  {
    detail::lock_guard<value_type> h(this);
    if(h._f)
      h._f->set_error(e);
    else
      _storage.set_error(e);
  }
  void set_exception(exception_type e) noexcept(std::is_nothrow_copy_constructible<exception_type>::value)
  {
    detail::lock_guard<value_type> h(this);
    if(h._f)
      h._f->set_exception(e);
    else
      _storage.set_exception(e);
  }
  template<typename E> void set_exception(E &&e)
  {
    set_exception(make_exception_ptr(std::forward<E>(e)));
  }
  
  // Not supported right now
  // void set_value_at_thread_exit(R v);
  // void set_exception_at_thread_exit(R v);
};

// TODO: promise<void>, promise<R&> specialisations
// TODO: future<void>, future<R&> specialisations

/*! \class future
\brief Lightweight next generation future with N4399 Concurrency TS extensions

http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4399.html
*/
template<typename R> class future : protected monad<R, true>
{
  typedef monad<R, true> monad_type;
  friend class promise<R>;
  friend struct detail::lock_guard<R>;
public:
  typedef typename monad_type::value_type value_type;
  typedef typename monad_type::exception_type exception_type;
  typedef typename monad_type::error_type error_type;
  BOOST_STATIC_CONSTEXPR bool is_consuming=monad_type::is_consuming;
  typedef promise<value_type> promise_type;
private:
  bool _need_locks;  // Used to inhibit unnecessary atomic use, thus enabling constexpr collapse
  char _lock_buffer[sizeof(spinlock<bool>)];
  spinlock<bool> &_lock() { return *(spinlock<bool> *)_lock_buffer; }
  promise_type *_promise;
protected:
  // Called by promise::get_future(), so currently thread safe
  BOOST_CXX14_CONSTEXPR future(promise_type *p) : monad_type(std::move(p->_storage)), _need_locks(p->_need_locks), _promise(p)
  {
    if(_need_locks) new (&_lock()) spinlock<bool>();
    p->_storage.set_future(this);
  }
public:
  //! \brief EXTENSION: constexpr capable constructor
  BOOST_CONSTEXPR future() : _need_locks(false), _promise(nullptr)
  {
    static_assert(std::is_move_constructible<value_type>::value, "Type must be move constructible to be used in a lightweight future");    
  }
  BOOST_CXX14_CONSTEXPR future(future &&o) noexcept(std::is_nothrow_move_constructible<monad_type>::value) : _need_locks(o._need_locks), _promise(nullptr)
  {
    if(_need_locks) new (&_lock()) spinlock<bool>();
    detail::lock_guard<value_type> h(&o);
    new(this) monad_type(std::move(o));
    if(o._promise)
    {
      _promise=o._promise;
      o._promise=nullptr;
      if(h._p)
        h._p->_storage.future_=this;
    }
  }
  future &operator=(future &&o) noexcept(std::is_nothrow_move_constructible<monad_type>::value)
  {
    // TODO FIXME: Only safe if both of these are noexcept
    this->~future();
    new (this) future(std::move(o));
    return *this;
  }
  future(const future &)=delete;
  future &operator=(const future &)=delete;
  ~future() noexcept(std::is_nothrow_destructible<monad_type>::value)
  {
    detail::lock_guard<value_type> h(this);
    if(h._p)
      h._p->_storage.reset();
    // Destroy myself before locks exit
    monad_type::reset();
  }
  
  void swap(future &o) noexcept(std::is_nothrow_move_constructible<monad_type>::value)
  {
    detail::lock_guard<value_type> h1(this), h2(&o);
    monad_type::swap(o._storage);
    if(h1._p)
      h1._p->_storage.future_=&o;
    if(h2._p)
      h2._p->_storage.future_=this;
  }
  
  // shared_future<value_type> share();  // TODO
  
  value_type get()
  {
    wait();
    detail::lock_guard<value_type> h(this);
    value_type ret(monad_type::get());
    if(h._p)
      h._p->_storage.reset();
    if(h._f)
      h._f->_promise=nullptr;
    return std::move(ret);
  }
  // value_type get_or(const value_type &);  // TODO
  // value_type get_or(value_type &&);  // TODO
  exception_type get_exception()
  {
    wait();
    detail::lock_guard<value_type> h(this);
    exception_ptr e(monad_type::get_exception());
    if(!e)
      return e;
    if(h._p)
      h._p->_storage.reset();
    if(h._f)
      h._f->_promise=nullptr;
    return e;
  }
  // Compatibility with Boost.Thread
  exception_type get_exception_ptr() { return get_exception(); }
  error_type get_error()
  {
    wait();
    detail::lock_guard<value_type> h(this);
    error_code e(monad_type::error());
    if(!e)
      return e;
    if(h._p)
      h._p->_storage.reset();
    if(h._f)
      h._f->_promise=nullptr;
    return e;
  }
  
  bool valid() const noexcept
  {
    return !!_promise;
  }
  using monad_type::is_ready;
  using monad_type::has_value;
  using monad_type::has_error;
  using monad_type::has_exception;
  
  void wait() const
  {
    if(!valid())
      throw future_error(future_errc::no_state);
    // TODO Actually sleep
    while(!monad_type::is_ready())
    {
    }
  }
  // template<class R, class P> future_status wait_for(const std::chrono::duration<R, P> &rel_time) const;  // TODO
  // template<class C, class D> future_status wait_until(const std::chrono::time_point<C, D> &abs_time) const;  // TODO
  
  // TODO Where F would return a future<future<...>>, we unwrap to a single future<R>
  // template<class F> typename std::result_of<F(future)>::type then(F &&f);
};

template<typename R> future<typename std::decay<R>::type> make_ready_future(R &&v)
{
  return future<typename std::decay<R>::type>(std::forward<R>(v));
}
template<typename R> future<R> make_errored_future(std::error_code v)
{
  return future<R>(v);
}
template<typename R> future<R> make_exceptional_future(std::exception_ptr v)
{
  return future<R>(v);
}

// TODO
// template<class InputIterator> ? when_all(InputIterator first, InputIterator last);
// template<class... Futures> ? when_all(Futures &&... futures);
// template<class Sequence> struct when_any_result;
// template<class InputIterator> ? when_any(InputIterator first, InputIterator last);
// template<class... Futures> ? when_any(Futures &&... futures);

// TODO packaged_task

#ifdef _MSC_VER
#undef value
#undef exception
#undef error
#undef future_
#endif

}
BOOST_SPINLOCK_V1_NAMESPACE_END

namespace std
{
  template<typename R> void swap(BOOST_SPINLOCK_V1_NAMESPACE::lightweight_futures::promise<R> &a, BOOST_SPINLOCK_V1_NAMESPACE::lightweight_futures::promise<R> &b)
  {
    a.swap(b);
  }
  template<typename R> void swap(BOOST_SPINLOCK_V1_NAMESPACE::lightweight_futures::future<R> &a, BOOST_SPINLOCK_V1_NAMESPACE::lightweight_futures::future<R> &b)
  {
    a.swap(b);
  }
}

#endif
