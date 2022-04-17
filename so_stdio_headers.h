#ifndef SO_STDIO_HEADERS_H
#define SO_STDIO_HEADERS_H

#define BUF_SIZE 4096
#define READ 0
#define WRITE 1
#define PIPE_READ	0
#define PIPE_WRITE	1

struct _so_file {
	int fd;
	int is_read, is_write, is_append, is_pipe;
	int feof, ferror;
	int last_operation;
	/*
	 * Indicates where the stream cursor is in the file
	 */
	long cursor;
	size_t  buffer_size;

	/*
	 * Indicates the last element read/written from/to the buffer
	 */
	size_t buffer_cursor;
	unsigned char buffer[BUF_SIZE];
};

SO_FILE *init_so_file(int fd, const char *mode);
void init_so_file_mode(SO_FILE *stream, const char *mode);
void reset_buffer(SO_FILE *stream);
ssize_t bring_to_buffer(SO_FILE *stream);
ssize_t flush_buffer(SO_FILE *stream);
#endif
