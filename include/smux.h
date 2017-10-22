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
 * \see{smux_config_in}, \see{smux_config_out}.
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
 * \brief                   configuration struct for sender
 *
 * See smux_init() for proper initialization.
 */
struct smux_config_send
{
    /**
     * \brief                   protocol settings
     *
     * This part defines protocol adjustments that NEED TO BE IDENTICAL on both sides of
     * the connection. They are not synchronized automatically. Consider keeping the default.
     */
    struct
    {
        /// the escape character (default: x01)
        char esc;
    } proto;

    /**
     * \brief                   buffer settings
     *
     * Required for proper use.
     */
    struct
    {
        /// buffer for outgoing data
        void *write_buf;
        size_t write_buf_size; ///< write_buf's size in bytes

        /**
         * \brief                   write function to copy send multiplexed stream to
         *                          NULL is acceptable
         *
         * If set, \see{smux_write} is used for sending data. If not, \see{smux_write_buf}.
         */
        smux_write_fn write_fn;
        /// data (file descriptor) to pass to write_fn
        void *write_fd;
    } buffer;

    // internal state
    struct
    {
        unsigned write_buf_head; // next character to write
        unsigned write_buf_tail; // next character to read
    } _internal;
};

/**
 * \brief                   configuration struct for receiver
 *
 * See smux_init() for proper initialization.
 */
struct smux_config_recv
{
    /**
     * \brief                   protocol settings
     *
     * This part defines protocol adjustments that NEED TO BE IDENTICAL on both sides of
     * the connection. They are not synchronized automatically. Consider keeping the default.
     */
    struct
    {
        /// the escape character (default: x01)
        char esc;
    } proto;

    /**
     * \brief                   buffer settings
     *
     * Required for proper use.
     */
    struct
    {
        /// buffer for incoming data (must be of 16 bytes at minimum)
        void *read_buf;
        size_t read_buf_size; ///< read_buf's size in bytes

        /**
         * \brief                   read function to read multiplexed copyless
         *
         * If set, use \see{smux_read()} to get data into the internal buffer. If not, use
         * \see{smux_read_buf()} instead.
         */
        smux_read_fn read_fn;
        /// data pointer to pass to read_fn
        void *read_fd;
    } buffer;

    // internal state
    struct
    {
        unsigned read_buf_head; // next character to write
        unsigned read_buf_tail; // next character to read

        smux_channel recv_ch;
        size_t recv_chars;
    } _internal;
};

/**
 * \brief                   set default values to all config variables and initialize
 *                          the internal state
 * \param[in,out] cs        pointer to a smux_config_send or NULL
 * \param[in,out] cr        pointer to a smux_config_recv or NULL
 *
 * After initialization, the config can be edited. At least, write and read buffers must
 * be defined (buffer.write_buf and buffer.read_buf including their size fields).
 *
 * Setting buffer.write_fn and buffer.read_fn (and their data pointers) is recommended.
 */
void smux_init(struct smux_config_send *cs, struct smux_config_recv *cr);

/**
 * \brief                   free ressources allocated by smux_init()
 * \param[in,out] cs        pointer to an initialized smux_config_send or NULL
 * \param[in,out] cr        pointer to an initialized smux_config_recv or NULL
 */
void smux_free(struct smux_config_send *cs, struct smux_config_recv *cr);



/**
 * \brief                   send data over a virtual channel
 * \param[in,out] config    initialized smux_config_send
 * \param ch                virtual channel
 * \param buf               data
 * \param count             number of bytes
 * \retval >0               number of bytes copied to the write buffer
 * \retval  0               write buffer is full
 *
 * This functions only copies the data into the internal write buffer (using the
 * SMUX protocol). To actually write data out, \see{smux_write} and \see{smux_write_buf}.
 */
size_t smux_send(struct smux_config_send *config, smux_channel ch, const void *buf, size_t count);

/**
 * \brief                   receive data from a virtual channel
 * \param[in,out] config    initialized smux_config_recv
 * \param[out] ch           channel that the data was read from
 * \param[out] buf          buffer for receiving
 * \param count             size of buffer
 * \retval >0               num bytes written to buf
 * \retval  0               no data received
 *
 * This function extracts data from the read buffer. If the buffer is not empty, the channel
 * number is written to *ch, the data is copied to buf and the number of copied bytes
 * is returned. Otherwise, the function returns 0 and leaves the channel number untouched.
 *
 * Bevore calling this function, the internal read buffer must be filled, \see{smux_read}
 * and \see{smux_read_buf}.
 */
size_t smux_recv(struct smux_config_recv *config, smux_channel *ch, void *buf, size_t count);



/**
 * \brief                   write multiplexed data using the configured write function
 * \param[in,out] config    initialized smux_config_send
 * \retval >0               number of bytes LEFT in the buffer after the write function
 *                          returned 0 (i.e., after it could not write anything anymore)
 * \retval  0               all bytes in the write buffer have been written
 * \retval <0               error (from the write function)
 *
 * Can be called if config->buffer.write_fn is not NULL to write the send buffer contents.
 * The configured write function is called several times until either
 * the write buffer is empty (return 0), the write function returned 0 (return >0) or
 * the write function has signalled an error (return <0).
 *
 * In case of an error, the buffer state is reverted, so the failing write is undone.
 */
ssize_t smux_write(struct smux_config_send *config);

/**
 * \brief                   extract multiplexed data from the internal to an external buffer
 * \param[in,out] config    initialized smux_config_send
 * \param buf               buffer to write to
 * \param count             size of buf
 * \retval >0               number of copied bytes
 * \retval  0               write buffer empty
 *
 * TODO: not yet implemented
 */
//size_t smux_write_buf(struct smux_config_send *config, void *buf, size_t count);



/**
 * \brief                   read multiplexed data into the internal buffer using the
 *                          configured read function
 * \param[in,out] config    initialized smux_config_recv
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
ssize_t smux_read(struct smux_config_recv *config);

/**
 * \brief                   read multiplexed data into the internal buffer from an
 *                          external buffer
 * \param[in,out] config    initialized smux_config_recv
 * \param buf               buffer to read from
 * \param count             maximum number of characters in buf
 * \retval >0               number of copied bytes
 * \retval  0               read buffer completely filled
 */
size_t smux_read_buf(struct smux_config_recv *config, const void *buf, size_t count);

#ifdef __cplusplus
}
#endif

#endif // ifndef _SMUX_H_INCLUDED_
