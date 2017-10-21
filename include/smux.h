/// \file smux.h
#ifndef _SMUX_H_INCLUDED_
#define _SMUX_H_INCLUDED_

#include <stddef.h> // size_t, ssize_t

// TODO: is ssize_t platform-independent?
#ifndef __ssize_t_defined
typedef signed int ssize_t;
# define __ssize_t_defined
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file smux.h
 *
 * SMUX is a library to multiplex binary character streams (Stream MUltiplexer).
 * It is intended to be used on both sides of a communication channel that cannot
 * do much more than transmitting bytes in both directions, such as UART serial
 * interfaces. SMUX allows to use multiple virtual channels to be transmitted
 * over such a channel.
 *
 * Virtual channels are identified by integers 0<=ch<256, so there are at maximum
 * 256 channels. Channel 0 is special: it is assumed by default if no escape
 * sequence is detected. Thus, it is a kind of fallback channel and can be used,
 * e.g., to see boot messages of a device that finally initializes SMUX
 * communication.
 *
 * The SMUX protocol is designed to be widely stateless, allowing fast recover if
 * one of the connected devices fails/reboots.
 *
 * The library needs to be configured before starting to communicate, see
 * \ref{smux_config}.
 *
 * Naming convention: All functions/data structures used to transmit or receive data
 * from the (physical) communication channel have a "write" or a "read",
 * respectively, in their names (because you will use something like the *nix write()
 * and read() functions to transmit or receive the data). Conversely, functions/data
 * structures that refer to the virtual communication channels use the keywords "send"
 * and "recv" to avoid confusion.
 */

/// identifiers of virtual channels
typedef unsigned char smux_channel;

enum
{
    smux_channel_default = 0,       ///< default channel in absense of channel escape sequences
    smux_channel_min = 0,           ///< lowest valid channel
    smux_channel_max = 0xFF,        ///< highest valid channel
};

/**
 * \brief                   function for writing multiplexed data
 * \param fd                pointer to user data (e.g., a file descriptor)
 * \param buf               data buffer to send
 * \param count             number of bytes in buf
 * \retval >=0              number of bytes written
 * \retval  <0              error
 */
typedef ssize_t (*smux_write_fn)(void *fd, const void *buf, size_t count);

/**
 * \brief                   function for reading multiplexed data
 * \param fd                pointer to user data (e.g., a file descriptor)
 * \param[out] buf          buffer to write read data to
 * \param count             maximum number of bytes to read
 * \retval >count           more bytes available to read, count bytes copied to buf
 * \retval >=0              number of read bytes
 * \retval  <0              error
 */
typedef ssize_t (*smux_read_fn)(void *fd, void *buf, size_t count);

/**
 * \brief                   configuration struct
 *
 * An smux_config struct contains the entire configuration and state of a smux connection. It
 * consists of three parts: the protocol settings, the buffer settings and internal state.
 *
 * See smux_init() for proper initialization.
 */
struct smux_config
{
    /**
     * \brief                   protocol settings
     *
     * This part defines protocol adjustments that NEED TO BE IDENTICAL on both sides of
     * the connection. To keep a smux connection stateless, they are not synced/compared
     * between both ends.
     */
    struct
    {
        /// the escape character (default: x01)
        char esc;
    } proto;

    /**
     * \brief                   buffer settings
     *
     * This part defines what is necessary to technically write and read multiplexed data.
     */
    struct
    {
        /// buffer for outgoing data
        void *write_buf;
        size_t write_buf_size; ///< write_buf's size in bytes

        /// buffer for incoming data
        void *read_buf;
        size_t read_buf_size; ///< read_buf's size in bytes

        /**
         * \brief                   write function to copy send multiplexed stream to
         *                          NULL is acceptable
         *
         * If NULL, characters will not be send automatically but need to be extracted
         * with polling. TODO: Implement this.
         */
        smux_write_fn write_fn;
        /// data (file descriptor) to pass to write_fn
        void *write_fd;

        /**
         * \brief                   read function to read multiplexed data from
         *                          NULL is acceptable
         *
         * If NULL, characters will not be received automatically but need to be inserted
         * explicitly. Alternatively, use smux_read_buf to directly copy bytes into the
         * read buffer.
         */
        smux_read_fn read_fn;
        /// data (file descriptor) to pass to read_fn
        void *read_fd;
    } buffer;

    /**
     * \brief                   internal state
     *
     * This internal part keeps necessary state across multiple calls to the smux
     * interface.
     * Applications shall not access it directly.
     */
    struct
    {
        // write ring buffer indexes
        unsigned write_buf_head; // next character to write
        unsigned write_buf_tail; // next character to read
        // read ring buffer indexes
        unsigned read_buf_head; // next character to write
        unsigned read_buf_tail; // next character to read
        // current receiver state
        smux_channel recv_ch;
        size_t recv_chars;
    } _internal;
};

/**
 * \brief                   set default values to all config variables and initialize
 *                          the internal state
 * \param[in,out] config    pointer to a smux_config
 *
 * After initialization, the config can be edited. At least, write and read buffers must
 * be defined (buffer.write_buf and buffer.read_buf including their size fields).
 * The buffers must be available during the entire time the library is used.
 * They have to be of at least 16 bytes each; 256 bytes or more is recommended.
 *
 * For simplicity, it is recommended to set buffer.write_fn and buffer.read_fn (and their
 * file descriptors) to transparently write and read multiplexed data.
 */
void smux_init(struct smux_config *config);

/**
 * \brief                   free ressources allocated by smux_init()
 * \param[in,out] config    pointer to an initialized smux_config
 */
void smux_free(struct smux_config *config);

/**
 * \brief                   send data over a virtual channel
 * \param[in,out] config    initialized smux_config
 * \param ch                virtual channel to use for sending
 * \param buf               data to send
 * \param count             number of bytes to send
 * \retval >0               number of bytes moved to the write buffer
 * \retval  0               write buffer is full
 *
 * This functions only copies the data into the internal write buffer (using the
 * SMUX protocol). It does not attempt to write data using the write function
 * (see smux_flush() for this).
 */
size_t smux_send(struct smux_config *config, smux_channel ch, const void *buf, size_t count);

/**
 * \brief                   receive data from a virtual channel
 * \param[in,out] config    initialized smux_config
 * \param[out] ch           channel that the data was read from
 * \param[out] buf          buffer to write received data to
 * \param count             size of the buffer
 * \retval >0               number of received characters written to buf
 * \retval  0               no data received
 *
 * This function extracts data from the read buffer. If the buffer is not empty, the channel
 * number is written to *ch, the data is copied to buf and the number of copied bytes
 * is returned. Otherwise, the function returns 0 and leaves the channel number untouched.
 */
size_t smux_recv(struct smux_config *config, smux_channel *ch, void *buf, size_t count);

/**
 * \brief                   write multiplexed data using the configured write function
 * \param[in,out] config    initialized smux_config
 * \retval >0               number of bytes LEFT in the buffer after the write function
 *                          returned 0 (i.e., after it could not write anything anymore)
 * \retval  0               all bytes in the write buffer have been written
 * \retval <0               error (from the write function)
 *
 * This function can be called if config->buffer.write_fn is not NULL to write the send
 * buffer contents. The configured write function is called several times until either
 * the write buffer is empty (return 0), the write function returned 0 (return >0) or
 * the write function has signalled an error (return <0).
 *
 * In case of an error, the buffer state is reverted, so the failing write is "undone".
 */
ssize_t smux_write(struct smux_config *config);

/**
 * \brief                   read multiplexed data into the internal buffer using the
 *                          configured read function
 * \param[in,out] config    initialized smux_config
 * \retval >0               remaining free space in the buffer
 * \retval  0               read buffer completely filled
 * \retval <0               error (from the read function)
 *
 * The function can be called if config->buffer.read_fn is not NULL to read data into
 * the read buffer. The read function is called several times until the read buffer
 * is full (return 0) if the read functions signals ability to serve more characters.
 * If the read function has signalled an error (return <0), no further reads are
 * attempted.
 */
ssize_t smux_read(struct smux_config *config);

/**
 * \brief                   read multiplexed data into the internal buffer from an
 *                          external buffer
 * \param[in,out] config    initialized smux_config
 * \param buf               buffer to read from
 * \param count             maximum number of characters in buf
 * \retval >0               number of copied bytes
 * \retval  0               read buffer completely filled
 */
size_t smux_read_buf(struct smux_config *config, const void* buf, size_t count);

#ifdef __cplusplus
}
#endif

#endif // ifndef _SMUX_H_INCLUDED_
