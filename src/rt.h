/// \file rt.h
#ifndef _RT_H_INCLUDED_
#define _RT_H_INCLUDED_

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <unordered_map>

#include <smux.hpp>

#include "file.h"
#include "error.h"

namespace smux_client
{
    /**
     * \brief                   smux client runtime system
     *
     * An object of this classes manages all channels, monitors their associated file descriptors
     * and copies the data to the right places.
     */
    class runtime_system
    {
        public:
            enum
            {
                RECEIVE_BUFFER_SIZE = 2048, ///< size of receive buffers in runtime_system
                SMUX_BUFFER_SIZE = 4096, ///< size of buffers in smux
            };

            /**
             * \brief                   ctor
             * \param master_in         file to read smux data from
             * \param master_out        file to write smux data to
             */
            runtime_system(std::unique_ptr<file> master_in, std::unique_ptr<file> master_out)
                : _smux(SMUX_BUFFER_SIZE)
            {
                _setup_shutdown_pipe();

                if(master_in)
                    _master.in = std::make_shared<half_channel>(0, std::move(master_in));
                if(master_out)
                    _master.out = std::make_shared<half_channel>(0, std::move(master_out));
            }

            /**
             * \brief                   ctor
             * \param master            file to read/write smux data
             */
            runtime_system(std::unique_ptr<file> master)
                : _smux(SMUX_BUFFER_SIZE)
            {
                _setup_shutdown_pipe();

                if(master)
                {
                    _master.in = std::make_shared<half_channel>(0, std::move(master));
                    _master.out = _master.in;
                }
            }

            /**
             * \brief                   add a new channel with different input and output files
             * \param ch                the associated smux channel
             * \param in                file for reading
             * \param out               file for writing
             */
            void add_channel(smux_channel ch, std::unique_ptr<file> in, std::unique_ptr<file> out)
            {
                auto& channel = _channels[ch];
                if(in)
                {
                    auto half_channel_in = std::make_shared<half_channel>(ch, std::move(in));
                    channel.in = half_channel_in;
                }
                if(out)
                {
                    auto half_channel_out = std::make_shared<half_channel>(ch, std::move(out));
                    channel.out = half_channel_out;
                }
            }

            /**
             * \brief                   add a new channel with equal input and ouput file
             * \param ch                the associated channel
             * \param io                file for reading and writing
             */
            void add_channel(smux_channel ch, std::unique_ptr<file> io)
            {
                if(io)
                {
                    auto half_channel_io = std::make_shared<half_channel>(ch, std::move(io));

                    auto& channel = _channels[ch];
                    channel.in = half_channel_io;
                    channel.out = half_channel_io;
                }
            }

            /**
             * \brief                   main function
             *
             * The function begins to monitor all file descriptors of all channels and copies the
             * data to the right places. It is called after the configuration has been parsed and
             * this object is initialized using add_channel(). Usually, it does not return
             * (except in case of error).
             */
            void run();

            /**
             * \brief                   asynchronous shutdown
             *
             * This function is meant to be called from a signal handler.
             */
            void shutdown();

            /**
             * \brief                   dtor
             */
            ~runtime_system();

        private:
            using buffer = std::vector<char>;

            /// sets of file descriptors for select()
            struct fd_sets
            {
                struct __inner
                {
                    fd_set fs;
                    int fd_max;

                    __inner()
                    {
                        zero();
                    }
                    void zero()
                    {
                        FD_ZERO(&fs);
                        fd_max = 0;
                    }
                    bool is_set(file_descriptor fd) const
                    {
                        return FD_ISSET(fd, &fs);
                    }
                    void clear(file_descriptor fd)
                    {
                        FD_CLR(fd, &fs);
                    }
                    void set(file_descriptor fd)
                    {
                        FD_SET(fd, &fs);
                        fd_max = std::max(fd, fd_max);
                    }
                }
                read, write, except;
            };

            /// file descriptors of a single file
            struct file_fds
            {
                file_descriptor_set read, write, except;
            };

            /// one half of a channel (in or out)
            struct half_channel
            {
                smux_channel const ch;
                std::unique_ptr<file> const fl;
                file_fds fds;
                buffer out_buffer; // characters to be written soon

                half_channel(smux_channel ch_, std::unique_ptr<file> fl_)
                    : ch(ch_)
                      , fl(std::move(fl_))
                {}
            };

            /// two half channels form a channel (in can be equal to out)
            struct channel
            {
                std::shared_ptr<half_channel> in;
                std::shared_ptr<half_channel> out;
            };

            // map with definitions of all channels
            using channel_map = std::unordered_map<smux_channel, channel>;
            // lookup map to fastly find a half_channel on a select event
            using fd_map = std::unordered_map<file_descriptor, half_channel*>;

            // (de)muxer
            smux::connection _smux;
            // master file
            channel _master;
            // all channels go here
            channel_map _channels;
            // map of all file descriptors to their half channels
            fd_map _fm;
            // file descritor sets for select()
            fd_sets _fs;
            // pipe that becomes readable on shutdown signal reception
            int _pipesig_r = -1, _pipesig_w;


            /**
             * \brief                   update a fd_map and fd_sets wrt. a single file
             * \param hc                channel to re-query for file descriptors
             */
            void _update_fds(half_channel& hc);

            // wrapper for function above taking care of calling it once or twice for channels with separate in/out
            void _update_fds(channel& c)
            {
                if(c.in)
                    _update_fds(*c.in);
                if(c.in != c.out && c.out)
                    _update_fds(*c.out);
            }

            // init _pipesig_r/w
            void _setup_shutdown_pipe();
    };
} // namespace smux_client

#endif
