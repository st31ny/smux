// files.cpp
#include "file.h"
#include "file_factory.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace smux_client
{
    static void assert_config(bool f, char const* msg)
    {
        if(!f)
            throw config_error(msg);
    }

    /**
     * \brief                   base class for files with one file descriptor for reading and
     *                          another one for writing
     *
     * Derived classes can simply overwrite _fdr and _fdw. Using the same file descriptor reading/
     * writing or only one of the two is acceptable.
     */
    class simple_file : public file
    {
        public:
            simple_file()
                : _fdr(fd_nil)
                , _fdw(fd_nil)
                , _eof(false)
            {
            }

            virtual bool read_event(fd_type const&) override
            {
                // select() signaled readiness for reading -> reading always possible
                return true;
            }

            virtual bool write_event(fd_type const&) override
            {
                // select() signaled readiness for writing -> writing always possible
                return true;
            }

            virtual void exception_event(fd_type const&) override
            {
            }

            virtual void select_fds(fd_set_type& read_fds, fd_set_type& write_fds,
                    fd_set_type& except_fds, bool data_present) override
            {
                if(!_eof && _fdr != fd_nil)
                    read_fds.insert(_fdr);
                if(data_present && _fdw != fd_nil)
                   write_fds.insert(_fdw);
                (void)except_fds;
            }

            virtual std::size_t read(void* buf, std::size_t count) override
            {
                if(_fdr == fd_nil)
                    return 0;
                auto ret = ::read(_fdr, buf, count);
                if(ret < 0)
                    throw system_error(errno);
                if(ret == 0) // eof -> avoid further read events
                    _eof = true;
                return static_cast<std::size_t>(ret);
            }

            virtual std::size_t write(const void* buf, std::size_t count) override
            {
                if(_fdw == fd_nil)
                    return 0;
                auto ret = ::write(_fdw, buf, count);
                if(ret < 0)
                    throw system_error(errno);
                return static_cast<std::size_t>(ret);
            }

            virtual ~simple_file()
            {
                if(_fdr != fd_nil)
                    close(_fdr);
                if(_fdw != _fdr && _fdw != fd_nil)
                    close(_fdw);
                // ignore errors (do not throw in dtors)
            }
        protected:
            int _fdr, _fdw;
            bool _eof;
    };

    // simple file for reading/writing
    class regular_file : public simple_file
    {
        public:
            regular_file(file_type const&, file_mode m, file_args const& args)
            {
                int flags = 0;

                // parse arguments
                assert_config(args.size() >= 1, "one argument required");
                assert_config(args.size() <= 2, "only two arguments supported");
                switch(m)
                {
                    case file_mode::io:
                        flags = O_RDWR | O_CREAT; break;
                    case file_mode::in:
                        flags = O_RDONLY; break;
                    case file_mode::out:
                        flags = O_WRONLY | O_CREAT; break;
                    default:
                        throw config_error("invalid file mode");
                }
                if(args.size() > 1)
                {
                    assert_config(args[1] == "a" || args[1] == "t", "optional flag value unsupported");
                    if(args[1] == "a")
                        flags |= O_APPEND;
                    else
                    if(args[1] == "t")
                        flags |= O_TRUNC;
                }

                // open file
                fd_type fd = open(args[0].c_str(), flags, 0666);
                if(fd == -1)
                    throw system_error(errno);
                _fdr = fd;
                _fdw = fd;
            }
    };

    // stdio
    class stdio_file : public simple_file
    {
        public:
            stdio_file(file_type const&, file_mode m, file_args const& args)
            {
                assert_config(args.size() == 0, "no arguments supported");

                bool fstdin = m != file_mode::out;
                bool fstdout = m != file_mode::in;
                if(fstdin)
                {
                    _fdr = dup(STDIN_FILENO);
                    if(_fdr == -1)
                        throw system_error(errno);
                }
                if(fstdout)
                {
                    _fdw = dup(STDOUT_FILENO);
                    if(_fdw == -1)
                        throw system_error(errno);
                }
            }
    };

    // register file types
    static register_file_type<regular_file> regular_file_registrar("file");
    static register_file_type<stdio_file> stdin_file_registrar("stdio");


} // smux_client
