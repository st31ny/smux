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
    /**
     * \brief                   read/write mode for files
     */
    enum class file_mode : int
    {
        in  = 4, ///< open file for reading
        out = 2, ///< open file for writing
        io  = 6, ///< open file for reading and writing
    };

    /// type name of files
    using file_type = std::string;

    /**
     * \brief                   parameters for a file
     */
    struct file_def
    {
        file_type type; ///< file type
        file_mode mode; ///< open mode of the file
        std::string arg; ///< argument for file
    };

    /**
     * \brief                   file factory function type
     * \param fl_def            file definition
     * \return                  newly created file
     * \throw                   config_error (arguments invalid)
     * \throw                   system_error (problem allocating resources)
     */
    using file_factory_fn = std::unique_ptr<file> (file_def const& fl_def);

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
             * \param def               definition structure
             * \return                  newly created file
             * \see                     file_factory_fn
             */
            std::unique_ptr<file> create(file_def const& def)
            {
                try
                {
                    return _registry.at(def.type)(def);
                } catch(std::out_of_range&)
                {
                    throw config_error("unknown file type '" + def.type + "'");
                }
                // never reached
                return nullptr;
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
            std::unique_ptr<file> create(file_def const& fl_def)
            {
                return std::unique_ptr<file>(new file_t(fl_def));
            }
    };

} // namespace smux_client

#endif // ifndef _FILE_FACTORY_H_INCLUDED_
