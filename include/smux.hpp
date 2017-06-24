/// \file smux.hpp
#ifndef _SMUX_HPP_INCLUDED_
#define _SMUX_HPP_INCLUDED_

#include <istream>
#include <ostream>
#include <sstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "smux.h"

/**
 * \file smux.hpp
 *
 * This file contains the header-only C++ wrapper for the smux library (\see{smux.h}).
 *
 * The main class, smux::connection handels buffers and wraps all settings of \ref smux_config.
 */

namespace smux
{
    /**
     * \brief                   exception base class
     *
     * All exceptions thrown by this library are derived from this class.
     */
    class error : public std::runtime_error
    {
        public:
            using std::runtime_error::runtime_error;
    };

    /**
     * \brief                   configuration error
     */
    class config_error : public error
    {
        public:
            using error::error;
    };

    /**
     * \brief                   function type for write function
     * \param buf               data buffer to write
     * \param count             number of bytes in buf
     * \retval >=0              number of written bytes
     * \retval  <0              error
     */
    using write_fn = ssize_t (const void* buf, size_t count);

    /**
     * \brief                   function type for read function
     * \param buf               data buffer to read into
     * \param count             capacity of buf
     * \retval >=0              number of read bytes
     * \retval  <0              error
     */
    using read_fn = ssize_t (void* buf, size_t count);

    /**
     * \brief                   wrapper for a smux configuration/connection
     */
    class connection
    {
        public:
            /**
             * \brief                   ctor
             * \pararm buf_size         write/read buffer size (must be >= 16)
             */
            connection(size_t buf_size = 1024)
            {
                if(buf_size < 16)
                    throw config_error("smux requires a buffer size of at least 16 bytes");
                _write_buf.resize(buf_size);
                _read_buf.resize(buf_size);

                // init smux config
                smux_init(&_smux);
                _smux.buffer.write_buf = _write_buf.data();
                _smux.buffer.write_buf_size = _write_buf.size();
                _smux.buffer.write_fn = writer;
                _smux.buffer.write_fd = reinterpret_cast<void*>(&_write_fn);
                _smux.buffer.read_buf = _read_buf.data();
                _smux.buffer.read_buf_size = _read_buf.size();
                _smux.buffer.read_fn = reader;
                _smux.buffer.read_fd = reinterpret_cast<void*>(&_read_fn);
            }

            /**
             * \brief                   set the write function
             *
             * The write function is called by smux to actually transfer data.
             * You usually want to set it.
             */
            void set_write_fn(std::function<write_fn> fn)
            {
                _write_fn = std::move(fn);
            }

            /**
             * \brief                   set the read function
             *
             * The read function is called by smux to actully read data before decoding.
             * You usually want to set it.
             */
            void set_read_fn(std::function<read_fn> fn)
            {
                _read_fn = std::move(fn);
            }

            /**
             * \brief                   get raw access to the smux config if you are brave
             * \return                  pointer to the used smux_config
             */
            smux_config* smux()
            {
                return &_smux;
            }

            // sorry, no copy
            connection(connection const&) = delete;
            connection& operator=(connection const&) = delete;
            // move invalidates _write_buf.data()
            connection(connection&&) = delete;
            connection& operator=(connection&&) = delete;

            /**
             * \brief                   dtor
             */
            ~connection()
            {
                smux_free(&_smux);
            }

        protected:
            template<class charT, class traits, class Alloc>
            friend class basic_ostream;
            template<class charT, class traits, class Alloc>
            friend class basic_istream;

            // adapter for write function
            static
            ssize_t writer(void* fd, const void *buf, size_t count)
            {
                auto& fn = *reinterpret_cast<std::function<write_fn>*>(fd);
                return fn(buf, count);
            }

            // adapter for read function
            static
            ssize_t reader(void* fd, void *buf, size_t count)
            {
                auto& fn = *reinterpret_cast<std::function<read_fn>*>(fd);
                return fn(buf, count);
            }

            smux_config _smux;
            using buffer = std::vector<char>;
            buffer _write_buf, _read_buf;
            std::function<write_fn> _write_fn;
            std::function<read_fn> _read_fn;
    };

    /**
     * \brief                   stream to send data
     */
    template <class charT, class traits = std::char_traits<charT>, class Alloc = std::allocator<charT>>
    class basic_ostream : public std::basic_ostream<charT, traits>
    {
        public:
            /**
             * \brief                   ctor
             * \param smux              a smux connection to send on
             * \param ch                initial channel (can be changed with channel())
             */
            basic_ostream(connection& smux, smux_channel ch = 0)
                : std::basic_ostream<charT, traits>(&_sb) // this is safe according to the standard
                , _sb(&smux._smux, ch)
            {
            }

            /**
             * \brief                   set the channel to send on
             * \param ch                new channel
             */
            void channel(smux_channel ch)
            {
                _sb._ch = ch;
            }

            /**
             * \brief                   get the current channel
             * \return                  current channel
             */
            smux_channel channel() const
            {
                return _sb._ch;
            }

        private:
            struct sendbuf : std::basic_stringbuf<charT, traits, Alloc>
            {
                sendbuf(smux_config* smux, smux_channel ch)
                    : _smux(smux), _ch(ch)
                {}

                int sync() override
                {
                    auto data = this->str();
                    ssize_t ret = 0;
                    while(data.size() > 0)
                    {
                        ret = smux_send(_smux, _ch, data.data(), data.size());
                        if(ret <= 0) break;
                        data.erase(0, ret); // remove the part that was written
                        ret = smux_flush(_smux);
                        if(ret <= 0) break;
                    }
                    // only keep data that could not be written
                    this->str(data);

                    if(ret < 0)
                        return -1;
                    return 0;
                }

                smux_config* _smux;
                smux_channel _ch;
            } _sb;
    };

    /**
     * \brief                   class to receive data
     */
    template <class charT, class traits = std::char_traits<charT>, class Alloc = std::allocator<charT>>
    class basic_istream : public std::basic_istream<charT, traits>
    {
        public:
            /**
             * \brief                   ctor
             * \param smux              a smux connection to receive from
             */
            basic_istream(connection& smux)
                : std::basic_istream<charT, traits>(&_rb) // this is safe according to the standard
                , _rb(&smux._smux)
            {
            }

            /**
             * \brief                   get the current channel
             * \return                  current channel
             */
            smux_channel channel() const
            {
                return _rb._ch;
            }

            /**
             * \brief                   reset the stream state to possibly receive from another stream
             *
             * Whenever a channel is exhausted (no more characters available), this stream will signal
             * EOF. This function is necessary to acknowledge this condition, reset the EOF state and
             * become ready to read from another stream.
             *
             * Attention: Quering the current channel() directly after a call to reset() does not
             * necessarily yield the new channel (e.g., if no new characters have been received yet),
             * so always first retrieve characters before checking the channel.
             */
            void reset()
            {
                this->clear();
                _rb._fReset = true;
            }

        private:
            struct recvbuf : std::basic_streambuf<charT, traits>
            {
                recvbuf(smux_config* smux)
                    : _smux(smux)
                    , _size(0)
                    , _fReset(true)
                {
                    _buf.resize(_smux->buffer.read_buf_size);
                    // make the entire capacity available
                    if(_buf.capacity() > _buf.size())
                        _buf.resize(_buf.capacity());

                    auto end = _buf.data() + _buf.size();
                    this->setg(end, end, end);
                }

                typename traits::int_type underflow() override
                {
                    if(this->gptr() == this->egptr())
                    {
                        // TODO: Is this wise here? Otherwise, smux_recv hangs,
                        // waiting for characters.
                        if(!_fReset)
                            return traits::eof();

                        if(_size == 0) // no new data available?
                        {
                            ssize_t result = smux_recv(_smux, &_chNext, _buf.data(), _buf.size());
                            if(result >= 0)
                            {
                                _size = result;
                            } else
                            {
                                // error
                                // TODO: signal error to stream
                                return traits::eof();
                            }
                        }

                        // make data available iff in reset state
                        if(_fReset)
                        {
                            _fReset = false;
                            _ch = _chNext;
                            this->setg(_buf.data(), _buf.data(), _buf.data() + _size);
                            _size = 0;
                        }
                    }

                    return this->gptr() == this->egptr()
                        ? traits::eof()
                        : traits::to_int_type(*this->gptr());
                }

                smux_config* _smux;
                std::vector<charT, Alloc> _buf;
                smux_channel _ch, _chNext; // current and next channel
                size_t _size; // valid chars in _buf
                bool _fReset; // true to accept a new channel
            } _rb;
    };

    using ostream = basic_ostream<char>;
    using istream = basic_istream<char>;

} // namespace smux

#endif // ifndef _SMUX_HPP_INCLUDED_
