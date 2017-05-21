// smux.c
#include <smux.h>

// adjust ring buffer index
#define ADJRBI(i, s) ((i) >= (s) ? (i)-(s) : (i))
// used elements in the buffer (given head, tail and size)
#define RBUSED(h, t, s) ((h)>=(t) ? (h)-(t) : (s)-(t)+(h))

// protocol properties
#define PROTO_CHANNEL_BYTES 1
#define PROTO_SIZE_BYTES 2
#define PROTO_MAX_SIZE ((1l<<PROTO_SIZE_BYTES*8)-1)

void smux_init(struct smux_config *config)
{
    // just zero-initialize everything
    static struct smux_config zero_config;
    *config = zero_config;

    // non-zero stuff
    config->proto.esc = '\x01';
}

void smux_free(struct smux_config *config)
{
    (void)config;
    // no clean-up needed currently
}

ssize_t smux_send(struct smux_config *config, smux_channel ch, const void *buf, size_t count)
{
    char *write_buf = (char*)config->buffer.write_buf;
    unsigned write_buf_head = config->_internal.write_buf_head;
    unsigned write_buf_tail = config->_internal.write_buf_tail;
    size_t write_buf_size = config->buffer.write_buf_size;
    size_t write_buf_used = RBUSED(write_buf_head, write_buf_tail, write_buf_size);
    unsigned size_field = 0; // size field position in write_buf
    char esc = config->proto.esc;

    char* input_buf = (char*)buf;
    size_t count_copied = 0; // copied chars from input_buf

    // catch trivial case
    if(count <= 0) return 0;

    // limit size
    if(count > PROTO_MAX_SIZE)
        count = PROTO_MAX_SIZE;

    // insert channel & size field
    if(ch != 0)
    {
        // enough space for escape byte, the channel and size fields?
        if(write_buf_used + 1 + PROTO_CHANNEL_BYTES + PROTO_SIZE_BYTES >= write_buf_size - 1)
            return 0;

        // esc char
        write_buf[write_buf_head] = esc;
        write_buf_head = ADJRBI(write_buf_head + 1, write_buf_size);
        // channel field (1 byte)
        write_buf[write_buf_head] = (char)ch;
        write_buf_head = ADJRBI(write_buf_head + 1, write_buf_size);
        // keep space for size field (2 bytes)
        size_field = write_buf_head;
        write_buf_head = ADJRBI(write_buf_head + 2, write_buf_size);
    }

    // copy the data in while not full
    for(; (count_copied < count) && (ADJRBI(write_buf_head + 1, write_buf_size) != write_buf_tail); count_copied++)
    {
        if(input_buf[count_copied] == esc)
        {
            // enough space for two characters?
            if(ADJRBI(write_buf_head + 2, write_buf_size) == write_buf_tail)
                break;
            // escape the escape char with esc + '\0'
            write_buf[write_buf_head] = esc;
            write_buf_head = ADJRBI(write_buf_head + 1, write_buf_size);
            write_buf[write_buf_head] = 0;
            write_buf_head = ADJRBI(write_buf_head + 1, write_buf_size);
        } else
        {
            write_buf[write_buf_head] = input_buf[count_copied];
            write_buf_head = ADJRBI(write_buf_head + 1, write_buf_size);
        }
    }

    // write size field (big endian)
    if(ch != 0)
    {
        write_buf[size_field] = (char)(count_copied >> 8);
        size_field = ADJRBI(size_field + 1, write_buf_size);
        write_buf[size_field] = (char)(count_copied & 0xFF);
    }

    // write head index back
    config->_internal.write_buf_head = write_buf_head;

    return count_copied;
}

ssize_t smux_recv(struct smux_config *config, smux_channel *ch, void *buf, size_t count)
{
    char *read_buf = (char*)config->buffer.read_buf;
    unsigned read_buf_head = config->_internal.read_buf_head;
    unsigned read_buf_tail = config->_internal.read_buf_tail;
    size_t read_buf_size = config->buffer.read_buf_size;
    size_t read_buf_used = RBUSED(read_buf_head, read_buf_tail, read_buf_size);
    smux_read_fn read_fn = config->buffer.read_fn;
    int fd = config->buffer.read_fd;
    smux_channel recv_ch = config->_internal.recv_ch; // current channel
    size_t recv_chars = config->_internal.recv_chars; // remaining payload chars
    char esc = config->proto.esc;

    ssize_t ret = 1;
    unsigned end;
    char* output_buf = (char*)buf;
    size_t count_copied = 0;

    // enough data available or fetch?
    while(read_buf_used == 0 || // buffer empty
       (read_buf[read_buf_tail] == esc && read_buf_used == 1) || // incomplete escape sequence
       (read_buf[read_buf_tail] == esc && read_buf[ADJRBI(read_buf_tail + 1, read_buf_size)] != 0 &&
          read_buf_used < 1 + PROTO_CHANNEL_BYTES + PROTO_SIZE_BYTES) // incomplete ch esc seq
    )
    {
        if(!read_fn)
            return 0;

        // leave one byte space to ensure separation of buffer full/empty
        end = read_buf_tail <= read_buf_head ? read_buf_size : read_buf_tail - 1;
        if(read_buf_tail == 0)
            end = read_buf_size - 1;
        ret = read_fn(fd, (void*)(read_buf + read_buf_head), end - read_buf_head);
        if(ret <= 0)
            break;

        read_buf_head = ADJRBI(read_buf_head + ret, read_buf_size);
        read_buf_used += ret;
    }

    config->_internal.read_buf_head = read_buf_head;
    // on error, always return for propor error handling
    // if ret == 0, not enough chars were received
    if(ret <= 0)
        return ret;

    // read buffer byte wise
    *ch = recv_ch;
    while(count_copied < count && // caller buffer not full
        (recv_ch == 0 || count_copied < recv_chars) && // we are still receiving for recv_ch
        read_buf_head != read_buf_tail // receive buffer not empty
    )
    {
        if(read_buf[read_buf_tail] == esc)
        {
            read_buf_tail = ADJRBI(read_buf_tail + 1, read_buf_size);

            // another char to decode esc seq?
            if(read_buf_tail == read_buf_head)
                break;

            if(read_buf[read_buf_tail] == 0) // just escape of esc char
            {
                output_buf[count_copied++] = esc;
                read_buf_tail = ADJRBI(read_buf_tail + 1, read_buf_size);
            } else // channel information
            {
                // enough to decode channel and size?
                if(RBUSED(read_buf_head, read_buf_tail, read_buf_size) < PROTO_CHANNEL_BYTES + PROTO_SIZE_BYTES)
                    break;

                recv_ch = read_buf[read_buf_tail];
                read_buf_tail = ADJRBI(read_buf_tail + 1, read_buf_size);
                recv_chars = (read_buf[read_buf_tail] << 8) & 0xFF00;
                read_buf_tail = ADJRBI(read_buf_tail + 1, read_buf_size);
                recv_chars |= read_buf[read_buf_tail] & 0xFF;
                read_buf_tail = ADJRBI(read_buf_tail + 1, read_buf_size);

                if(count_copied > 0)
                {
                    // in case we already copied payload, stop to separate channels
                    break;
                } else
                {
                    // otherwise, report the channel
                    *ch = recv_ch;
                }
            }
        } else // normal case (no esc char)
        {
            output_buf[count_copied++] = read_buf[read_buf_tail];
            read_buf_tail = ADJRBI(read_buf_tail + 1, read_buf_size);
        }
    }

    // write indexes and receiver state back
    config->_internal.read_buf_tail = read_buf_tail;
    config->_internal.read_buf_head = read_buf_head;
    config->_internal.recv_ch = recv_ch;
    config->_internal.recv_chars = recv_chars;

    return count_copied;
}

ssize_t smux_flush(struct smux_config *config)
{
    char *write_buf = (char*)config->buffer.write_buf;
    unsigned write_buf_head = config->_internal.write_buf_head;
    unsigned write_buf_tail = config->_internal.write_buf_tail;
    size_t write_buf_size = config->buffer.write_buf_size;
    smux_write_fn write_fn = config->buffer.write_fn;
    int fd = config->buffer.write_fd;

    ssize_t ret;
    unsigned end;

    if(write_fn)
    {
        while(write_buf_head != write_buf_tail)
        {
            // end of area to transmit
            end = write_buf_tail < write_buf_head ? write_buf_head : write_buf_size;

            ret = write_fn(fd, (void*)(write_buf + write_buf_tail), end - write_buf_tail);
            if(ret <= 0)
                break;

            write_buf_tail = ADJRBI(write_buf_tail + ret, write_buf_size);
        }

        // write new tail index back
        config->_internal.write_buf_tail = write_buf_tail;

        if(ret < 0) // error?
            return ret;
    }
    return RBUSED(write_buf_head, write_buf_tail, write_buf_size);
}
