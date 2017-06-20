/// \file file_factory.h
#ifndef _FILE_FACTORY_H_INCLUDED_
#define _FILE_FACTORY_H_INCLUDED_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "file.h"
#include "errors.h"

namespace smux_client
{
    /// argument list for file creation
    using file_args = std::vector<std::string>;
    /// type name of files
    using file_type = std::string;

    /**
     * \brief                   file factory function type
     * \param type              type name of the requested function
     * \param args              the list of arguments for this file
     * \throw                   config_error (arguments invalid)
     * \throw                   system_error (problem allocating resources)
     */
    using file_factory_fn = std::unique_ptr<file> (file_type const& type, file_args const& args);

    /**
     * \brief                   registry for file types
     */
    class file_factory
    {
        public:
            /**
             * \brief                   get the application-wide factory instance
             * \return                  pointer to the system-wide factory object and never nullptr
             */
            static
            file_factory* get() noexcept
            {
                 return &_instance;
            }

            /**
             * \brief                   register a factory function
             * \param type              name of the file type
             * \param fn                factory function
             */
            void reg(file_type const& type, std::function<file_factory_fn> fn)
            {
                _registry[type] = fn;
            }

            /**
             * \brief                   create a file
             * \param type              name of the file type
             * \param args              arguments for the file type factory function
             * \see                     file_factory_fn
             */
            std::unique_ptr<file> create(file_type const& type, file_args const& args)
            {
                return _registry.at(type)(type, args);
            }

        private:
            static file_factory _instance;

            std::map<std::string, std::function<file_factory_fn>> _registry;
    };

    /**
     * \brief                   helper template to create and register a factory function
     *
     * The template expects a file's ctor taking a std::string (the file type's name) and
     * a file_args as parameters.
     */
    template<class file_t>
    class register_file_type
    {
        public:
            /**
             * \brief                   ctor
             * \param type              name of the file type to register as
             */
            register_file_type(file_type const& type)
            {
                 file_factory::get()->reg(type, &create);
            }

            /**
             * \brief                   factory function to actually create a file
             */
            static
            std::unique_ptr<file> create(file_type const& type, file_args const& args)
            {
                return std::unique_ptr<file>(new file_t(type, args));
            }
    };

} // namespace smux_client

#endif // ifndef _FILE_FACTORY_H_INCLUDED_
