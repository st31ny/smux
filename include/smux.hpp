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
 * The main class, smux::connection handels buffers and wraps all settings of \see smux_config.
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

    /// default buffer size
    enum { DEFAULT_BUF_SIZE = 1024 };

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
     * \retval >count           more bytes available to read, count bytes copied to buf
     * \retval >=0              number of read bytes
     * \retval  <0              error
     */
    using read_fn = ssize_t (void* buf, size_t count);

    /**
     * \brief                   wrapper for a smux sender
     */
    class sender
    {
        public:
            /**
             * \brief                   ctor
             * \param buf_size          writer buffer size (must be >= 16)
             */
            sender(size_t buf_size = DEFAULT_BUF_SIZE)
            {
                if(buf_size < 16)
                    throw config_error("smux requires a buffer size of at least 16 bytes");
                _buf.resize(buf_size);

                // init smux config
                smux_init(&_smux, nullptr);
                _smux.buffer.write_buf = _buf.data();
                _smux.buffer.write_buf_size = _buf.size();
                _smux.buffer.write_fn = writer;
                _smux.buffer.write_fd = reinterpret_cast<void*>(&_write_fn);
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
             * \brief                   low-level send function
             * \see                     smux_send
             *
             * Consider using smux::ostream instead.
             */
            size_t send(smux_channel ch, const void *buf, size_t count)
            {
                return smux_send(&_smux, ch, buf, count);
            }

            /**
             * \brief                   low-level write function
             * \see                     smux_write
             */
            ssize_t write()
            {
                return smux_write(&_smux);
            }

            /**
             * \brief                   low-level write_buf function
             * \see                     smux_write_buf
             * TODO: not yet implemented
             */
            //size_t write_buf(void *buf, size_t count);

            /**
             * \brief                   get the internal config
             */
            smux_config_send* smux()
            {
                return &_smux;
            }

            // sorry, no copy
            sender(sender const&) = delete;
            sender& operator=(sender const&) = delete;
            // move invalidates _write_buf.data()
            sender(sender&&) = delete;
            sender& operator=(sender&&) = delete;

            /**
             * \brief                   dtor
             */
            ~sender()
            {
                smux_free(&_smux, nullptr);
            }

        protected:
            template<class charT, class traits, class Alloc>
            friend class basic_ostream;

            // adapter for write function
            static
            ssize_t writer(void* fd, const void *buf, size_t count)
            {
                auto& fn = *reinterpret_cast<std::function<write_fn>*>(fd);
                return fn(buf, count);
            }

            smux_config_send _smux;
            using buffer = std::vector<char>;
            buffer _buf;
            std::function<write_fn> _write_fn;
    };

    /**
     * \brief                   wrapper for a smux receiver
     */
    class receiver
    {
        public:
            /**
             * \brief                   ctor
             * \param buf_size          reader buffer size (must be >= 16)
             */
            receiver(size_t buf_size = DEFAULT_BUF_SIZE)
            {
                if(buf_size < 16)
                    throw config_error("smux requires a buffer size of at least 16 bytes");
                _buf.resize(buf_size);

                // init smux config
                smux_init(nullptr, &_smux);
                _smux.buffer.read_buf = _buf.data();
                _smux.buffer.read_buf_size = _buf.size();
                _smux.buffer.read_fn = reader;
                _smux.buffer.read_fd = reinterpret_cast<void*>(&_read_fn);
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
             * \brief                   low-level receive function
             * \see                     smux_recv
             *
             * Consider using smux::istream instead.
             */
            size_t recv(smux_channel *ch, void *buf, size_t count)
            {
                return smux_recv(&_smux, ch, buf, count);
            }

            /**
             * \brief                   low-level read function
             * \see                     smux_read
             */
            ssize_t read()
            {
                return smux_read(&_smux);
            }

            /**
             * \brief                   low-level read_buf function
             * \see                     smux_read_buf
             */
            size_t read_buf(const void* buf, size_t count)
            {
                return smux_read_buf(&_smux, buf, count);
            }

            /**
             * \brief                   get the internal config
             */
            smux_config_recv* smux()
            {
                return &_smux;
            }

            // sorry, no copy
            receiver(receiver const&) = delete;
            receiver& operator=(receiver const&) = delete;
            // move invalidates _write_buf.data()
            receiver(receiver&&) = delete;
            receiver& operator=(receiver&&) = delete;

            /**
             * \brief                   dtor
             */
            ~receiver()
            {
                smux_free(nullptr, &_smux);
            }

        protected:
            template<class charT, class traits, class Alloc>
            friend class basic_istream;

            // adapter for read function
            static
            ssize_t reader(void* fd, void *buf, size_t count)
            {
                auto& fn = *reinterpret_cast<std::function<read_fn>*>(fd);
                return fn(buf, count);
            }

            smux_config_recv _smux;
            using buffer = std::vector<char>;
            buffer _buf;
            std::function<read_fn> _read_fn;
    };

    /**
     * \brief                   smux connection containing a sender and a receiver
     */
    class connection : public sender, public receiver
    {
        public:
            /**
             * \brief                   ctor
             * \param write_buf_size    writer buffer size (must be >= 16)
             * \param read_buf_size     reader buffer size (must be >= 16)
             */
            connection(size_t write_buf_size = DEFAULT_BUF_SIZE, size_t read_buf_size = DEFAULT_BUF_SIZE)
                : sender(write_buf_size)
                , receiver(read_buf_size)
            {}
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
             * \param smux              a smux sender to send on
             * \param ch                initial channel (can be changed with channel())
             */
            basic_ostream(sender& smux, smux_channel ch = 0)
                : std::basic_ostream<charT, traits>(&_sb) // this is safe according to the standard
                , _sb(&smux._smux, ch)
            {
            }

            /**
             * \brief                   set the channel to send on
             * \param ch                new channel
             *
             * Flushes before setting the channel.
             */
            void channel(smux_channel ch)
            {
                _sb.sync();
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
                sendbuf(smux_config_send* smux, smux_channel ch)
                    : _smux(smux), _ch(ch)
                {}

                int sync() override
                {
                    auto data = this->str();
                    size_t ret = 0;
                    while(data.size() > 0)
                    {
                        // TODO: byte order
                        ret = smux_send(_smux, _ch, reinterpret_cast<const void*>(data.data()), data.size()*sizeof(charT));
                        if(ret <= 0) break;
                        data.erase(0, ret); // remove the part that was written
                        ret = smux_write(_smux);
                        if(ret <= 0) break;
                    }
                    // only keep data that could not be written
                    this->str(data);

                    return 0;
                }

                smux_config_send* _smux;
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
             * \param smux              a smux receiver to receive from
             */
            basic_istream(receiver& smux)
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
                recvbuf(smux_config_recv* smux)
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
                        // TODO: Is this wise here? Otherwise, smux_read might hang,
                        // waiting for characters.
                        if(!_fReset)
                            return traits::eof();

                        if(_size == 0) // no new data available?
                        {
                            if(smux_read(_smux) < 0)
                            {
                                // error
                                // TODO: signal error to stream
                                return traits::eof();
                            }

                            // TODO: byte order
                            _size = smux_recv(_smux, &_chNext, reinterpret_cast<void*>(_buf.data()), _buf.size()*sizeof(charT));
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

                smux_config_recv* _smux;
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
