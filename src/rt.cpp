// rt.cpp
#include <cstring>
#include <iostream>

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>

#include "rt.h"

using namespace smux_client;

void runtime_system::run()
{
    buffer buf;

    // master files
    half_channel* master_in = _master.in.get();
    half_channel* master_out = _master.out.get();
    if(master_in)
    {
        // we cannot tell if more characters are available (read(2) does not tell us), but main loop
        // will call smux.read again if necessary
        _smux.set_read_fn([master_in](void* buf, size_t count) { return master_in->fl->read(buf, count); });
    } else
    {
        // no master_in
        _smux.set_read_fn([](void*, size_t) { return 0; });
        std::clog << "Warning: no master read file: cannot receive data" << std::endl;
    }
    if(master_out)
    {
        _smux.set_write_fn([master_out](const void* buf, size_t count) { return master_out->fl->write(buf, count); });
    } else
    {
        // no master_out
        _smux.set_write_fn([](const void*, size_t count) { return count; });
        std::clog << "Warning: no master write file: cannot transmit data" << std::endl;
    }

    // initially fill _fm and _fs
    for(auto& channel : _channels)
    {
        _update_fds(channel.second);
    }
    // hook up the master
    _update_fds(_master);

    // main loop
    std::clog << "entering main loop" << std::endl;
    while(true)
    {
        fd_sets _fs_tmp = _fs;
        _fs_tmp.read.set(_pipesig_r); // setup signal notification pipe
        int nfds = std::max({_fs_tmp.read.fd_max, _fs_tmp.write.fd_max, _fs_tmp.except.fd_max}) + 1;
        if(0) std::clog << "calling select()..." << std::endl;
        int select_result = select(nfds, &_fs_tmp.read.fs, &_fs_tmp.write.fs, &_fs_tmp.except.fs, nullptr);
        if(0) std::clog << "called select()=" << select_result << std::endl;
        if(select_result < 0)
        {
            if(errno == EINTR) // tolerate interrupted syscall
                continue;
            throw system_error(errno);
        }

        // shutdown signal received?
        if(_fs_tmp.read.is_set(_pipesig_r))
        {
            std::clog << "\nshutdown signal received: exiting main loop" << std::endl;
            return;
        }

        for(file_descriptor fd = 0; fd < nfds; ++fd)
        {
            // find our half channel associated with fd
            auto _fm_it = _fm.find(fd);
            if(_fm_it == _fm.end())
                continue;
            auto& hc = *_fm_it->second;

            // read event
            if(_fs_tmp.read.is_set(fd) && hc.fl->read_event(fd))
            {
                if(&hc == master_in)
                {
                    if(0) std::clog << "master read event" << std::endl;
                    if(_smux.read() < 0)
                        throw system_error("reading into smux buffer failed");
                    if(0) std::clog << "_smux.read() done" << std::endl;

                    if(master_in->fl->eof())
                    {
                        std::clog << "\neof on master in -> shutdown" << std::endl;
                        return;
                    }

                    // receive data
                    std::size_t ret;
                    do
                    {
                        smux_channel ch;
                        buf.resize(RECEIVE_BUFFER_SIZE);
                        ret = _smux.recv(&ch, buf.data(), buf.size());

                        // forward data to the correct output
                        if(ret > 0)
                        {
                            if(_channels.count(ch))
                            {
                                auto& hc_out = _channels[ch].out;
                                if(hc_out)
                                {
                                    auto& out_buffer = hc_out->out_buffer;
                                    buf.resize(ret); // remember correct size
                                    if(out_buffer.size() == 0) // no data waiting currently?
                                        out_buffer = std::move(buf);
                                    else
                                        out_buffer.insert(out_buffer.end(), buf.begin(), buf.end());
                                    _update_fds(*hc_out);
                                    if(0) std::clog << "received data for channel " << static_cast<int>(ch) << std::endl;
                                }
                            } else // channel not existing
                            {
                                std::clog << "\nignoring data for channel " << static_cast<int>(ch) << std::endl;
                            }
                        }
                    } while(ret != 0);
                } else
                {
                    // a channel is ready to be read
                    if(0) std::clog << "read event on channel " << static_cast<int>(hc.ch) << ", fd=" << fd << std::endl;
                    buf.resize(RECEIVE_BUFFER_SIZE);

                    std::size_t ret = hc.fl->read(buf.data(), buf.size());
                    if(ret > 0)
                    {
                        std::clog << '<' << static_cast<int>(hc.ch) << std::flush;
                        // forward data to smux
                        smux::ostream out(_smux, hc.ch);
                        out.write(buf.data(), ret);
                        out.flush();
                    }
                }
            }

            // write event
            if(_fs_tmp.write.is_set(fd) && hc.fl->write_event(fd))
            {
                if(&hc == master_out)
                {
                    if(0) std::clog << "master write event" << std::endl;
                    // master is written directly as it is the bottleneck anyway
                } else
                {
                    // a channel is ready to be written
                    if(0) std::clog << "write event on channel " << static_cast<int>(hc.ch) << ", fd=" << fd << std::endl;
                    std::clog << '>' << static_cast<int>(hc.ch) << std::flush;
                    if(hc.out_buffer.size() > 0)
                    {
                        std::size_t ret = hc.fl->write(hc.out_buffer.data(), hc.out_buffer.size());
                        auto begin = hc.out_buffer.begin();
                        hc.out_buffer.erase(begin, begin + ret);
                    }
                }
            }

            // exception
            if(_fs_tmp.except.is_set(fd))
            {
                std::clog << "\nexcept event on " << fd << std::endl;

                // call exception handler
                hc.fl->exception_event(fd);
            }

            // give the file a chance to update its fd sets
            _update_fds(hc);
        }
    }
}

void runtime_system::shutdown()
{
    // signal event by writing to the pipe
    if(_pipesig_w >= 0)
    {
        // send a char to wakeup select()
        static const char dump = 0;
        write(_pipesig_w, &dump, 1);
    }
}

runtime_system::~runtime_system()
{
    // close signal notification pipe
    if(_pipesig_r >= 0)
        close(_pipesig_r);
    if(_pipesig_w >= 0)
        close(_pipesig_w);
}

void runtime_system::_update_fds(half_channel& hc)
{
    // ask file for its file descriptors
    file_descriptor_set read_fds, write_fds, except_fds;
    hc.fl->select_fds(read_fds, write_fds, except_fds, hc.out_buffer.size() != 0);

    // remove all old file descriptors
    for(auto const& fds : {hc.fds.read, hc.fds.write, hc.fds.except})
    {
        for(auto const& fd : fds)
        {
            _fm.erase(fd);

            // we assume that a fd uniquely belongs to a single file
            _fs.read.clear(fd);
            _fs.write.clear(fd);
            _fs.except.clear(fd);
        }
    }

    // add the new definitions
    for(auto const& fd : read_fds)
    {
        _fm[fd] = &hc;
        _fs.read.set(fd);
    }
    for(auto const& fd : write_fds)
    {
        _fm[fd] = &hc;
        _fs.write.set(fd);
    }
    for(auto const& fd : except_fds)
    {
        _fm[fd] = &hc;
        _fs.except.set(fd);
    }

    // finally, remember the new sets
    hc.fds.read = std::move(read_fds);
    hc.fds.write = std::move(write_fds);
    hc.fds.except = std::move(except_fds);
}

void runtime_system::_setup_shutdown_pipe()
{
    std::clog << "initialize shutdown pipe" << std::endl;
    int pipefd[2];
    if(pipe(pipefd))
        std::clog << "error initializing shutdown notification: " << std::strerror(errno) << std::endl;
    else
    {
        _pipesig_r = pipefd[0];
        _pipesig_w = pipefd[1];
    }
}
