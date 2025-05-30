#include <fc/asio.hpp>
#include <fc/thread/thread.hpp>
#include <boost/thread.hpp>
#include <fc/log/logger.hpp>

namespace fc {
  namespace asio {
    namespace detail {
        void read_write_handler( const promise<size_t>::ptr& p, const boost::system::error_code& ec, size_t bytes_transferred ) {
            if( !ec ) p->set_value(bytes_transferred);
            else {
            //   elog( "%s", boost::system::system_error(ec).what() );
            //   p->set_exception( fc::copy_exception( boost::system::system_error(ec) ) );
                if( ec == boost::asio::error::operation_aborted )
                {
                  p->set_exception( fc::exception_ptr( new fc::canceled_exception( 
                          FC_LOG_MESSAGE( error, "${message} ", ("message", boost::system::system_error(ec).what())) ) ) );
                }
                else if( ec == boost::asio::error::eof  )
                {
                  p->set_exception( fc::exception_ptr( new fc::eof_exception( 
                          FC_LOG_MESSAGE( error, "${message} ", ("message", boost::system::system_error(ec).what())) ) ) );
                }
                else
                {
                 // elog( "${message} ", ("message", boost::system::system_error(ec).what()));
                  p->set_exception( fc::exception_ptr( new fc::exception( 
                          FC_LOG_MESSAGE( error, "${message} ", ("message", boost::system::system_error(ec).what())) ) ) );
                }
            }
        }
        void read_write_handler_ec( promise<size_t>* p, boost::system::error_code* oec, const boost::system::error_code& ec, size_t bytes_transferred ) {
            p->set_value(bytes_transferred);
            *oec = ec;
        }
        void error_handler( const promise<void>::ptr& p, 
                              const boost::system::error_code& ec ) {
            if( !ec ) p->set_value();
            else
            {
                if( ec == boost::asio::error::operation_aborted )
                {
                  p->set_exception( fc::exception_ptr( new fc::canceled_exception( 
                          FC_LOG_MESSAGE( error, "${message} ", ("message", boost::system::system_error(ec).what())) ) ) );
                }
                else if( ec == boost::asio::error::eof  )
                {
                  p->set_exception( fc::exception_ptr( new fc::eof_exception( 
                          FC_LOG_MESSAGE( error, "${message} ", ("message", boost::system::system_error(ec).what())) ) ) );
                }
                else
                {
                 // elog( "${message} ", ("message", boost::system::system_error(ec).what()));
                  p->set_exception( fc::exception_ptr( new fc::exception( 
                          FC_LOG_MESSAGE( error, "${message} ", ("message", boost::system::system_error(ec).what())) ) ) );
                }
            }
        }

        void error_handler_ec( promise<boost::system::error_code>* p, 
                              const boost::system::error_code& ec ) {
            p->set_value(ec);
        }

        template<typename EndpointType, typename IteratorType>
        void resolve_handler( 
                             const typename promise<std::vector<EndpointType> >::ptr& p,
                             const boost::system::error_code& ec, 
                             IteratorType itr) {
            if( !ec ) {
                std::vector<EndpointType> eps;
                while( itr != IteratorType() ) {
                    eps.push_back(*itr);
                    ++itr;
                }
                p->set_value( eps );
            } else {
                //elog( "%s", boost::system::system_error(ec).what() );
                //p->set_exception( fc::copy_exception( boost::system::system_error(ec) ) );
                p->set_exception( 
                    fc::exception_ptr( new fc::exception( 
                        FC_LOG_MESSAGE( error, "process exited with: ${message} ", 
                                        ("message", boost::system::system_error(ec).what())) ) ) );
            }
        }
    }
    boost::asio::io_service& default_io_service(bool cleanup) {
        static boost::asio::io_service       io;
        static boost::asio::io_service::work the_work(io);
        static boost::thread                 io_t([=] 
               { 
                 try { 
                   fc::thread::current().set_name("asio");  
                   io.run(); 
                 }
                 catch(...)
                 {
                   elog( "unexpected asio exception" );
                 }
               } 
               );

        return io;
    }

    namespace tcp {
        std::vector<boost::asio::ip::tcp::endpoint> resolve( const std::string& hostname, const std::string& port) {
            resolver res( fc::asio::default_io_service() );
            promise<std::vector<boost::asio::ip::tcp::endpoint> >::ptr p( new promise<std::vector<boost::asio::ip::tcp::endpoint> >() );
            res.async_resolve( boost::asio::ip::tcp::resolver::query(hostname,port), 
                             boost::bind( detail::resolve_handler<boost::asio::ip::tcp::endpoint,resolver_iterator>, p, _1, _2 ) );
            return p->wait();;
        }
    }
    namespace udp {
                std::vector<udp::endpoint> resolve( resolver& r, const std::string& hostname, const std::string& port) {
                resolver res( fc::asio::default_io_service() );
                promise<std::vector<endpoint> >::ptr p( new promise<std::vector<endpoint> >() );
                res.async_resolve( resolver::query(hostname,port), 
                                    boost::bind( detail::resolve_handler<endpoint,resolver_iterator>, p, _1, _2 ) );
                return p->wait();
        }
    }
  
} } // namespace fc::asio
