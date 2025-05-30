#pragma once
#include <fc/utility.hpp>
#include <fc/time.hpp>
#include <fc/shared_ptr.hpp>
#include <fc/exception/exception.hpp>
#include <fc/thread/spin_yield_lock.hpp>
#include <fc/optional.hpp>

namespace fc {
  class abstract_thread;
  struct void_t{};
  class priority;
  class thread;

  namespace detail {
     class completion_handler {
       public:
          virtual ~completion_handler(){};
          virtual void on_complete( const void* v, const fc::exception_ptr& e ) = 0;
     };
     
     template<typename Functor, typename T>
     class completion_handler_impl : public completion_handler {
       public:
         completion_handler_impl( Functor&& f ):_func(fc::move(f)){}
         completion_handler_impl( const Functor& f ):_func(f){}
     
         virtual void on_complete( const void* v, const fc::exception_ptr& e ) {
           _func( *static_cast<const T*>(v), e);
         }
       private:
         Functor _func;
     };
     template<typename Functor>
     class completion_handler_impl<Functor,void> : public completion_handler {
       public:
         completion_handler_impl( Functor&& f ):_func(fc::move(f)){}
         completion_handler_impl( const Functor& f ):_func(f){}
         virtual void on_complete( const void* v, const fc::exception_ptr& e ) {
           _func(e);
         }
       private:
         Functor _func;
     };
  }

  class promise_base : public virtual retainable{
    public:
      typedef fc::shared_ptr<promise_base> ptr;
      promise_base(const char* desc="?");

      const char* get_desc()const;
                   
      void cancel();
      bool ready()const;
      bool error()const;

      void set_exception( const fc::exception_ptr& e );

    protected:
      void _wait( const microseconds& timeout_us );
      void _wait_until( const time_point& timeout_us );
      void _enqueue_thread();
      void _dequeue_thread();
      void _notify();
      void _set_timeout();
      void _set_value(const void* v);

      void _on_complete( detail::completion_handler* c );
      ~promise_base();

    private:
      friend class  thread;
      friend struct context;
      friend class  thread_d;

      bool                        _ready;
      mutable spin_yield_lock     _spin_yield;
      thread*                     _blocked_thread;
#ifndef NDEBUG
      unsigned                    _blocked_fiber_count;
#endif
      time_point                  _timeout;
      fc::exception_ptr           _exceptp;
      bool                        _canceled;
      const char*                 _desc;
      detail::completion_handler* _compl;
  };

  template<typename T = void> 
  class promise : virtual public promise_base {
    public:
      typedef fc::shared_ptr< promise<T> > ptr;
      promise( const char* desc = "?" ):promise_base(desc){}
      promise( const T& val ){ set_value(val); }
      promise( T&& val ){ set_value(fc::move(val) ); }
    
      const T& wait(const microseconds& timeout = microseconds::maximum() ){
        this->_wait( timeout );
        return *result;
      }
      const T& wait_until(const time_point& tp ) {
        this->_wait_until( tp );
        return *result;
      }

      void set_value( const T& v ) {
        result = v;
        _set_value(&*result);
      }

      void set_value( T&& v ) {
        result = fc::move(v);
        _set_value(&*result);
      }

      template<typename CompletionHandler>
      void on_complete( CompletionHandler&& c ) {
        _on_complete( new detail::completion_handler_impl<CompletionHandler,T>(fc::forward<CompletionHandler>(c)) );
      }
    protected:
      optional<T> result;
      ~promise(){}
  };

  template<>
  class promise<void> : virtual public promise_base {
    public:
      typedef fc::shared_ptr< promise<void> > ptr;
      promise( const char* desc = "?" ):promise_base(desc){}
      promise( const void_t& ){ set_value(); }
    
      void wait(const microseconds& timeout = microseconds::maximum() ){
        this->_wait( timeout );
      }
      void wait_until(const time_point& tp ) {
        this->_wait_until( tp );
      }

      void set_value(){ this->_set_value(nullptr); }
      void set_value( const void_t&  ) { this->_set_value(nullptr); }

      template<typename CompletionHandler>
      void on_complete( CompletionHandler&& c ) {
        _on_complete( new detail::completion_handler_impl<CompletionHandler,void>(fc::forward<CompletionHandler>(c)) );
      }
    protected:
      ~promise(){}
  };
  
  /**
   *  @brief a placeholder for the result of an asynchronous operation.
   *
   *  By calling future<T>::wait() you will block the current fiber until 
   *  the asynchronous operation completes.  
   *
   *  If you would like to use an asynchronous interface instead of the synchronous
   *  'wait' method you could specify a CompletionHandler which is a method that takes
   *  two parameters, a const reference to the value and an exception_ptr.  If the
   *  exception_ptr is set, the value reference is invalid and accessing it is
   *  'undefined'.  
   *
   *  Promises have pointer semantics, futures have reference semantics that
   *  contain a shared pointer to a promise.
   */
  template<typename T> 
  class future {
    public:
      future( const fc::shared_ptr<promise<T>>& p ):m_prom(p){}
      future( fc::shared_ptr<promise<T>>&& p ):m_prom(fc::move(p)){}
      future(){}

      operator const T&()const { return wait(); }
      
      /// @pre valid()
      /// @post ready()
      /// @throws timeout
      const T& wait( const microseconds& timeout = microseconds::maximum() )const {
        return m_prom->wait(timeout);
      }

      /// @pre valid()
      /// @post ready()
      /// @throws timeout
      const T& wait_until( const time_point& tp )const {
        return m_prom->wait_until(tp);
      }

      bool valid()const { return !!m_prom;       }

      /// @pre valid()
      bool ready()const { return m_prom->ready(); }

      /// @pre valid()
      bool error()const { return m_prom->error(); }

      void cancel()const { m_prom->cancel(); }

      /**
       * @pre valid()
       *
       * The given completion handler will be called from some
       * arbitrary thread and should not 'block'. Generally
       * it should post an event or start a new async operation.
       */
      template<typename CompletionHandler>
      void on_complete( CompletionHandler&& c ) {
        m_prom->on_complete( fc::forward<CompletionHandler>(c) );
      }
    private:
      friend class thread;
      fc::shared_ptr<promise<T>> m_prom;
  };

  template<>
  class future<void> {
    public:
      future( const fc::shared_ptr<promise<void>>& p ):m_prom(p){}
      future( fc::shared_ptr<promise<void>>&& p ):m_prom(fc::move(p)){}
      future(){}
      
      /// @pre valid()
      /// @post ready()
      /// @throws timeout
      void wait( const microseconds& timeout = microseconds::maximum() ){
        m_prom->wait(timeout);
      }

      /// @pre valid()
      /// @post ready()
      /// @throws timeout
      void wait_until( const time_point& tp ) {
        m_prom->wait_until(tp);
      }

      bool valid()const { return !!m_prom;       }

      /// @pre valid()
      bool ready()const { return m_prom->ready(); }

      /// @pre valid()
      bool error()const { return m_prom->error(); }

      void cancel()const { m_prom->cancel(); }

      template<typename CompletionHandler>
      void on_complete( CompletionHandler&& c ) {
        m_prom->on_complete( fc::forward<CompletionHandler>(c) );
      }

    private:
      friend class thread;
      fc::shared_ptr<promise<void>> m_prom;
  };
} 

