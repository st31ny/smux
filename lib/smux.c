// smux.c
#include <smux.h>
#include <string.h>

// adjust ring buffer index
static inline
unsigned ADJRBI(unsigned i, size_t s)
{
  return i >= s ? i - s : i;
}

static inline
size_t RBUSED(unsigned h, unsigned t, size_t s)
{
  return h >= t ? h - t : s - t + h;
}

// protocol properties
enum
{
  PROTO_CHANNEL_BYTES = 1,
  PROTO_SIZE_BYTES    = 2,
  PROTO_MAX_SIZE      = (1 << PROTO_SIZE_BYTES * 8) - 1,
};

void smux_init(struct smux_config_send *cs, struct smux_config_recv *cr)
{
    if(cr)
    {
        memset(cr, 0, sizeof(*cr));
        cr->proto.esc = '\x01';
    }
    if(cs)
    {
        memset(cs, 0, sizeof(*cs));
        cs->proto.esc = '\x01';
    }
}

void smux_free(struct smux_config_send *cs, struct smux_config_recv *cr)
{
    (void)cr;
    (void)cs;
    // no clean-up needed currently
}

size_t smux_send(struct smux_config_send *config, smux_channel ch, const void *buf, size_t count)
{
    char *write_buf = (char*)config->buffer.write_buf;
    unsigned wb_head = config->_internal.wb_head;
    unsigned wb_tail = config->_internal.wb_tail;
    size_t write_buf_size = config->buffer.write_buf_size;
    size_t write_buf_used = RBUSED(wb_head, wb_tail, write_buf_size);
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
        write_buf[wb_head] = esc;
        wb_head = ADJRBI(wb_head + 1, write_buf_size);
        // channel field (1 byte)
        write_buf[wb_head] = (char)ch;
        wb_head = ADJRBI(wb_head + 1, write_buf_size);
        // keep space for size field (2 bytes)
        size_field = wb_head;
        wb_head = ADJRBI(wb_head + 2, write_buf_size);
    }

    // copy the data in while not full
    for(; (count_copied < count) && (ADJRBI(wb_head + 1, write_buf_size) != wb_tail); count_copied++)
    {
        if(input_buf[count_copied] == esc)
        {
            // enough space for two characters?
            if(ADJRBI(wb_head + 2, write_buf_size) == wb_tail)
                break;
            // escape the escape char with esc + '\0'
            write_buf[wb_head] = esc;
            wb_head = ADJRBI(wb_head + 1, write_buf_size);
            write_buf[wb_head] = 0;
            wb_head = ADJRBI(wb_head + 1, write_buf_size);
        } else
        {
            write_buf[wb_head] = input_buf[count_copied];
            wb_head = ADJRBI(wb_head + 1, write_buf_size);
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
    config->_internal.wb_head = wb_head;

    return count_copied;
}

size_t smux_recv(struct smux_config_recv *config, smux_channel *ch, void *buf, size_t count)
{
    char *read_buf = (char*)config->buffer.read_buf;
    unsigned rb_head = config->_internal.rb_head;
    unsigned rb_tail = config->_internal.rb_tail;
    unsigned rb_tail_old;
    size_t read_buf_size = config->buffer.read_buf_size;
    smux_channel recv_ch = config->_internal.recv_ch; // current channel
    size_t recv_chars = config->_internal.recv_chars; // remaining payload chars
    char esc = config->proto.esc;

    char* output_buf = (char*)buf;
    size_t count_copied = 0;

    // read buffer byte wise
    *ch = recv_ch;
    while(count_copied < count && // caller buffer not full
        (recv_ch == 0 || recv_chars > 0) && // we are still receiving for recv_ch
        rb_head != rb_tail // receive buffer not empty
    )
    {
        rb_tail_old = rb_tail;
        if(read_buf[rb_tail] == esc)
        {
            rb_tail = ADJRBI(rb_tail + 1, read_buf_size);

            // another char to decode esc seq?
            if(rb_tail == rb_head)
            {
                rb_tail = rb_tail_old; // stuff read bytes back
                break;
            }

            if(read_buf[rb_tail] == 0) // just escape of esc char
            {
                output_buf[count_copied++] = esc;
                recv_chars -= 1;
                rb_tail = ADJRBI(rb_tail + 1, read_buf_size);
            } else // channel information
            {
                // enough to decode channel and size?
                if(RBUSED(rb_head, rb_tail, read_buf_size) < PROTO_CHANNEL_BYTES + PROTO_SIZE_BYTES)
                {
                    rb_tail = rb_tail_old;
                    break;
                }

                recv_ch = read_buf[rb_tail];
                rb_tail = ADJRBI(rb_tail + 1, read_buf_size);
                recv_chars = (read_buf[rb_tail] << 8) & 0xFF00;
                rb_tail = ADJRBI(rb_tail + 1, read_buf_size);
                recv_chars |= read_buf[rb_tail] & 0xFF;
                rb_tail = ADJRBI(rb_tail + 1, read_buf_size);

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
            output_buf[count_copied++] = read_buf[rb_tail];
            recv_chars -= 1;
            rb_tail = ADJRBI(rb_tail + 1, read_buf_size);
        }
    }

    // optimization: reset head and tail if buffer empty
    if(rb_tail == rb_head)
    {
        rb_tail = 0;
        rb_head = 0;
    }

    // write indexes and receiver state back
    config->_internal.rb_tail = rb_tail;
    config->_internal.rb_head = rb_head;
    if(recv_chars == 0)
        recv_ch = 0; // ensure correct channel if read everything
    config->_internal.recv_ch = recv_ch; // if channel == 0 recv_ch might underflow => but is always ignored in that case
    config->_internal.recv_chars = recv_chars;

    return count_copied;
}

ssize_t smux_write(struct smux_config_send *config)
{
    char *write_buf = (char*)config->buffer.write_buf;
    unsigned wb_head = config->_internal.wb_head;
    unsigned wb_tail = config->_internal.wb_tail;
    size_t write_buf_size = config->buffer.write_buf_size;
    smux_write_fn write_fn = config->buffer.write_fn;
    void *fd = config->buffer.write_fd;

    ssize_t ret, count;
    unsigned end;

    if(write_fn)
    {
        while(wb_head != wb_tail)
        {
            // end of area to transmit
            end = wb_tail < wb_head ? wb_head : write_buf_size;

            count = end - wb_tail;
            ret = write_fn(fd, (void*)(write_buf + wb_tail), count);
            if(ret <= 0)
                break;

            wb_tail = ADJRBI(wb_tail + (ret>count?count:ret), write_buf_size);
        }

        // optimization: if buffer is empty, reset head and tail to beginning
        if(wb_tail == wb_head)
        {
            config->_internal.wb_head = 0;
            wb_tail = 0;
        }

        // write new tail index back
        config->_internal.wb_tail = wb_tail;

        if(ret < 0) // error?
            return ret;
    }
    return RBUSED(wb_head, wb_tail, write_buf_size);
}

ssize_t smux_read(struct smux_config_recv *config)
{
    char *read_buf = (char*)config->buffer.read_buf;
    unsigned rb_head = config->_internal.rb_head;
    unsigned rb_tail = config->_internal.rb_tail;
    size_t read_buf_size = config->buffer.read_buf_size;
    smux_read_fn read_fn = config->buffer.read_fn;
    void *fd = config->buffer.read_fd;

    ssize_t ret = 0, count;
    unsigned end;

    if(read_fn)
    {
        // leave one byte space to ensure separation of buffer full/empty
        while(ADJRBI(rb_head + 1, read_buf_size) != rb_tail)
        {
            end = rb_tail <= rb_head ? read_buf_size : rb_tail - 1;
            if(rb_tail == 0)
                end = read_buf_size - 1;

            count = end - rb_head;
            ret = read_fn(fd, (void*)(read_buf + rb_head), count);
            if(ret <= 0)
                break;

            rb_head = ADJRBI(rb_head + (ret>count?count:ret), read_buf_size);
            // more bytes available?
            if(ret <= count)
                break;
        }

        // write new head index back
        config->_internal.rb_head = rb_head;

        if(ret < 0) // error?
            return ret;
    }
    // do not count the one byte that always has to stay free
    return read_buf_size - RBUSED(rb_head, rb_tail, read_buf_size) - 1;
}

size_t smux_read_buf(struct smux_config_recv *config, const void* buf, size_t count)
{
    char *read_buf = (char*)config->buffer.read_buf;
    unsigned rb_head = config->_internal.rb_head;
    unsigned rb_tail = config->_internal.rb_tail;
    size_t read_buf_size = config->buffer.read_buf_size;
    char *cbuf = (char*)buf;

    unsigned copied = 0;

    while(ADJRBI(rb_head + 1, read_buf_size) != rb_tail && copied < count)
    {
        read_buf[rb_head] = cbuf[copied];
        rb_head = ADJRBI(rb_head + 1, read_buf_size);
        copied++;
    }
    config->_internal.rb_head = rb_head;
    return copied;
}
