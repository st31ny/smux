// files.cpp
#include "file.h"
#include "file_factory.h"

#include <iostream>
#include <sstream>
#include <vector>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdio.h> // for perror
#include <stdlib.h> // for exit

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
            regular_file(file_def const& fl_def)
            {
                int flags = 0;
                auto& args = fl_def.args;

                // parse arguments
                assert_config(args.size() >= 1, "one argument required");
                assert_config(args.size() <= 2, "only two arguments supported");
                switch(fl_def.mode)
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
                flags |= O_CLOEXEC;
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
            stdio_file(file_def const& fl_def)
            {
                assert_config(fl_def.args.size() == 0, "no arguments supported");

                bool fstdin = fl_def.mode != file_mode::out;
                bool fstdout = fl_def.mode != file_mode::in;
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

    /**
     * \brief                   base class for exec
     *
     * start other program & use a socketpair for communication
     */
    class exec_base : public simple_file
    {
        public:
            exec_base()
            {}

            virtual ~exec_base()
            {
                // send SIGHUP to child process
                if(_child_pid)
                    kill(_child_pid, SIGHUP);
            }

        protected:
            /**
             * \brief                   fork and exec
             * \param path              program name or path
             * \param args              program arguments
             * \param mode              communication mode
             * \throw system_error
             */
            void init(std::string const& path, std::vector<std::string> const& args, file_mode mode);

        private:
            pid_t _child_pid = 0;
    };

    // simple program execution
    class exec : public exec_base
    {
        public:
            exec(file_def const& fl_def)
            {
                // split argument at spaces
                std::vector<std::string> args;
                std::string path;
                std::string tmp;
                std::istringstream is(fl_def.arg_string);
                is >> path;
                while(is >> tmp)
                    args.push_back(std::move(tmp));
                if(path.empty())
                    throw config_error("program path required");

                // execute the sub process
                init(path, args, fl_def.mode);
            }
    };

    // wrapper for exec for socat
    class socat : public exec_base
    {
        public:
            socat(file_def const& fl_def)
            {
                static const std::string socat_binary("socat");
                std::vector<std::string> args;
                // socat shall use stdin, stdout or both, depending on mode
                if(fl_def.mode == file_mode::in)
                    args.push_back("stdout");
                else if(fl_def.mode == file_mode::out)
                    args.push_back("stdin");
                else
                    args.push_back("stdio");
                // second argument describes other end of connection
                args.push_back(fl_def.arg_string);

                // execute!
                init(socat_binary, args, fl_def.mode);
            }
    };

    // register file types
    static register_file_type<regular_file> regular_file_registrar("file");
    static register_file_type<stdio_file> stdin_file_registrar("stdio");
    static register_file_type<exec> exec_registrar("exec");
    static register_file_type<socat> socat_registrar("socat");

    // implementation
    void exec_base::init(std::string const& path, std::vector<std::string> const& args, file_mode mode)
    {
        // init argument list
        char const* argv[args.size() + 2]; // space for program name + arguments + NULL
        argv[0] = path.c_str();
        unsigned i = 1;
        for(auto& arg : args)
        {
            argv[i++] = arg.c_str();
        }
        argv[i] = nullptr;

        // init communication
        int sv[2]; // [0]..parent side, [1]..child side
        if(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0)
            throw system_error(errno);
        int fd_parent = sv[0];
        int fd_chld = sv[1];

        // fork + exec
        pid_t pid = fork();
        if(pid == 0)
        { // child
            // close parent side
            close(fd_parent);

            // reopen stdin and stdout according to mode
            if(mode == file_mode::in)
            {
                // we do not write data out => close program's stdin
                int devNull = open("/dev/null", O_RDONLY);
                if(devNull != -1)
                {
                    dup2(devNull, STDIN_FILENO);
                    close(devNull);
                }
            } else
            {
                // we want to write data out => program needs stdin
                if(dup2(fd_chld, STDIN_FILENO) == -1)
                {
                    perror("establishing child communication failed");
                    exit(EXIT_FAILURE);
                }
            }
            if(mode == file_mode::out)
            {
                // we do not want to read data in => close program's stdout
                int devNull = open("/dev/null", O_WRONLY);
                if(devNull != -1)
                {
                    dup2(devNull, STDOUT_FILENO);
                    close(devNull);
                }

            } else
            {
                // we want to read data => program needs stdout
                if(dup2(fd_chld, STDOUT_FILENO) == -1)
                {
                    perror("establishing child communication failed");
                    exit(EXIT_FAILURE);
                }
            }

            // child side has been dupped (not needed anymore)
            close(fd_chld);

            // execute the program
            execvp(path.c_str(), (char* const*)argv);

            // if we reach this point, there was an error
            exit(EXIT_FAILURE);
        } else if(pid > 0)
        { // parent
            // close child side of communication and remember parent side
            close(fd_chld);
            if(mode != file_mode::out)
                _fdr = fd_parent;
            if(mode != file_mode::in)
                _fdw = fd_parent;

            // remember pid for later killing
            _child_pid = pid;
        } else
        {
            // error
            throw system_error("forking failed");
        }
    }

} // smux_client
