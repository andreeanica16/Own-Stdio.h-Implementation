#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include "so_stdio.h"
#include "so_stdio_headers.h"

/*
 * Initialise the SO_FILE structure
 */
SO_FILE *init_so_file(int fd, const char *mode)
{
	SO_FILE *stream;

	stream = malloc(sizeof(SO_FILE));
	if (stream == NULL)
		return NULL;
	memset(stream->buffer, 0, BUF_SIZE);

	stream->fd = fd;
	stream->buffer_size = 0;
	stream->buffer_cursor = 0;
	stream->feof = 0;
	stream->ferror = 0;
	stream->cursor = 0;
	stream->last_operation = -1;
	stream->is_pipe = 0;
	init_so_file_mode(stream, mode);

	return stream;
}

/*
 * Sets active is_read, is_write, is_append flags, based
 * on the mode provided by the user
 */
void init_so_file_mode(SO_FILE *stream, const char *mode)
{
	stream->is_read = 0;
	stream->is_write = 0;
	stream->is_append = 0;

	if (strchr(mode, 'r'))
		stream->is_read = 1;

	if (strchr(mode, 'w'))
		stream->is_write = 1;

	if (strchr(mode, 'a'))
		stream->is_append = 1;

	if (strchr(mode, '+'))
		stream->is_read = 1;

	if (strcmp(mode, "r+") == 0)
		stream->is_write = 1;
}

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *stream;
	int fd;

	if (strcmp(mode, "r") == 0)
		fd = open(pathname, O_RDONLY);
	else if (strcmp(mode, "r+") == 0)
		fd = open(pathname, O_RDWR);
	else if (strcmp(mode, "w") == 0)
		fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	else if (strcmp(mode, "w+") == 0)
		fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
	else if (strcmp(mode, "a") == 0)
		fd = open(pathname, O_WRONLY | O_APPEND | O_CREAT, 0644);
	else if (strcmp(mode, "a+") == 0)
		fd = open(pathname, O_RDWR | O_APPEND | O_CREAT, 0644);
	else
		return NULL;

	if (fd < 0)
		return NULL;

	stream = init_so_file(fd, mode);
	return stream;
}

int so_fclose(SO_FILE *stream)
{
	int rc;
	ssize_t bytes_written;

	/*
	 * If the stream is used for write and there is still
	 * unwritten information left in the buffer, flush the
	 * buffer first
	 */
	if ((stream->is_write || stream->is_append) &&
		stream->last_operation == WRITE &&
		stream->buffer_size > 0) {
		bytes_written = flush_buffer(stream);
		if (bytes_written == SO_EOF) {
			free(stream);
			return SO_EOF;
		}
	}

	rc = close(stream->fd);
	free(stream);

	if (rc < 0)
		return SO_EOF;

	return 0;
}

void reset_buffer(SO_FILE *stream)
{
	memset(stream->buffer, 0, BUF_SIZE);
	stream->buffer_cursor = 0;
	stream->buffer_size = 0;
}

ssize_t bring_to_buffer(SO_FILE *stream)
{
	ssize_t bytes_read = 0;

	reset_buffer(stream);

	bytes_read = read(stream->fd, stream->buffer, BUF_SIZE);

	/*
	 * If there was an I/O error
	 */
	if (bytes_read < 0) {
		stream->ferror = 1;
		return SO_EOF;
	}

	/*
	 * If we met EOF
	 */
	if (!bytes_read)
		stream->feof = 1;

	stream->buffer_size = bytes_read;
	return bytes_read;
}

ssize_t flush_buffer(SO_FILE *stream)
{
	size_t bytes_written = 0;
	ssize_t bytes_written_now;

	/*
	 * Write until the entire buffer is copied
	 */
	while (bytes_written < stream->buffer_size) {
		bytes_written_now = write(
			stream->fd,
			stream->buffer + bytes_written,
			stream->buffer_size - bytes_written
		);

		/*
		 * If there was an I/O error
		 */
		if (bytes_written_now <= 0) {
			stream->ferror = 1;
			return SO_EOF;
		}

		bytes_written += bytes_written_now;
	}

	reset_buffer(stream);

	return bytes_written;
}

int so_fgetc(SO_FILE *stream)
{
	ssize_t bytes_read;
	int read_char;

	if (!stream->is_read)
		return SO_EOF;

	/*
	 * If we met the EOF, it means we have nothing to
	 * read
	 */
	if (stream->feof)
		return SO_EOF;

	/*
	 * If we aren't at the end of the file and we need some info
	 * that is not in the buffer, bring more information to the buffer
	 */
	if (!stream->feof && stream->buffer_cursor >= stream->buffer_size) {
		bytes_read = bring_to_buffer(stream);

		if (bytes_read == SO_EOF)
			return SO_EOF;
		else if (stream->feof)
			return SO_EOF;
	}

	/*
	 * Read the information from the buffer
	 */
	read_char = stream->buffer[stream->buffer_cursor];
	stream->buffer_cursor += 1;
	stream->cursor += 1;
	stream->last_operation = READ;
	return read_char;
}

int so_fputc(int c, SO_FILE *stream)
{
	ssize_t bytes_written;

	if (!stream->is_write && !stream->is_append)
		return SO_EOF;

	/*
	 * If the buffer is full, flush the buffer
	 */
	if (stream->buffer_size == BUF_SIZE) {
		bytes_written = flush_buffer(stream);
		if (bytes_written == SO_EOF)
			return SO_EOF;
	}

	/*
	 * Write the information to the buffer
	 */
	stream->buffer[stream->buffer_size] = c;
	stream->buffer_size += 1;
	stream->cursor += 1;
	stream->last_operation = WRITE;
	return c;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	size_t i, j;
	size_t elements_read = 0;
	int rc;
	unsigned char *pointer = (unsigned char *)ptr;

	/*
	 * Read each element bythe by byte
	 */
	for (i = 0; i < nmemb; i++) {
		for (j = 0; j < size; j++) {
			rc = so_fgetc(stream);

			if (rc == SO_EOF)
				return stream->ferror ? 0 : elements_read;

			*(pointer + i * size + j) = rc;
		}
		elements_read += 1;
	}

	return elements_read;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	size_t i, j;
	size_t elements_written = 0;
	int rc;
	unsigned char c;
	unsigned char *pointer = (unsigned char *)ptr;

	/*
	 * Write each element bythe by byte
	 */
	for (i = 0; i < nmemb; i++) {
		for (j = 0; j < size; j++) {
			c = *(pointer + i * size + j);
			rc = so_fputc(c, stream);

			if (rc == SO_EOF)
				return 0;
		}
		elements_written += 1;
	}

	return elements_written;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	ssize_t bytes_written;
	int rc;

	/*
	 * If we move the cursor and we have some unwritten info
	 * in the buffer, first flush the buffer to the current
	 * position of the cursor, then move the cursor
	 */
	if (stream->is_write && stream->last_operation == WRITE) {
		bytes_written = flush_buffer(stream);
		if (bytes_written == SO_EOF)
			return SO_EOF;
	}
	reset_buffer(stream);
	stream->last_operation = -1;

	rc = lseek(stream->fd, offset, whence);
	if (rc == SO_EOF) {
		stream->ferror = 1;
		return SO_EOF;
	}

	stream->cursor = rc;
	stream->feof = 0;
	return 0;
}

long so_ftell(SO_FILE *stream)
{
	return stream->cursor;
}

int so_fflush(SO_FILE *stream)
{
	ssize_t bytes_written;

	/*
	 * Flush only has sense for write operations
	 */
	if (stream->last_operation != WRITE)
		return 0;

	bytes_written = flush_buffer(stream);
	if (bytes_written == SO_EOF)
		return SO_EOF;

	return 0;
}

int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_feof(SO_FILE *stream)
{
	return stream->feof;
}

int so_ferror(SO_FILE *stream)
{
	return stream->ferror;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	pid_t pid;
	int rc;
	int pipe_fds[2];
	int fd;
	SO_FILE *stream = NULL;
	char *const argv[] = {"sh", "-c", (char *const)command, NULL};

	/*
	 * If the mode provided by the user is different from
	 * "r" or "w", return error
	 */
	if (strcmp(type, "r") != 0 && strcmp(type, "w") != 0)
		return NULL;

	rc = pipe(pipe_fds);

	if (rc != 0)
		return NULL;

	pid = fork();
	switch (pid) {
	case -1:
		/* Fork failed, cleaning up... */
		close(pipe_fds[PIPE_READ]);
		close(pipe_fds[PIPE_WRITE]);
		return NULL;
	case 0:
		/* Child process */
		if (strcmp(type, "r") == 0) {
			close(pipe_fds[PIPE_READ]);
			dup2(pipe_fds[PIPE_WRITE], STDOUT_FILENO);
		} else if (strcmp(type, "w") == 0) {
			close(pipe_fds[PIPE_WRITE]);
			dup2(pipe_fds[PIPE_READ], STDIN_FILENO);
		}

		execvp("sh", (char *const *) argv);

		break;
	default:
		/* Parent process */
		if (strcmp(type, "r") == 0) {
			close(pipe_fds[PIPE_WRITE]);
			fd = pipe_fds[PIPE_READ];
		} else if (strcmp(type, "w") == 0) {
			close(pipe_fds[PIPE_READ]);
			fd = pipe_fds[PIPE_WRITE];
		}

		stream = init_so_file(fd, type);
		if (stream == NULL)
			return NULL;
		stream->is_pipe = pid;
		break;
	}

	return stream;
}

int so_pclose(SO_FILE *stream)
{
	int status;
	pid_t rc1, rc2;
	int pid = stream->is_pipe;

	if (pid == 0)
		return SO_EOF;

	rc1 = so_fclose(stream);
	rc2 = waitpid(pid, &status, 0);

	if (rc1 == -1 || rc2 == -1)
		return -1;

	return status;
}
