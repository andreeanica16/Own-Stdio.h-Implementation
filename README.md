# Stdio.h Implementation

## Introduction

Own implementation of the stdio library, which allows working with files. The library implements the SO_FILE structure (similar to the FILE in the standard C library), along with read / write functions. It will also provides buffering functionality.

## Description

Implementarea temei se bazeaza pe structura SO_FILE, care contine o
serie de informatii necesare:
The implementation is based on the SO_FILE structure, which contains the following information:
- fd = file descriptor
- is_read, is_write, is_append = flags indicating the operation type (read, write, append)
- error flags (feof, ferror)
- last_operation = remembers the type of the last operation performed on
handler (READ / WRITE), useful for handling
buffer for r +, w +, or a + operations
- cursor = cursor position in file
- a buffer in / from which information is written / read, to enlarge
performance - The buffer is characterized by 2 parameters, buffer_size
which retains the number of bytes in the buffer with useful information and
buffer cursor that holds the position of the handler inside the 
buffer.


## How the buffer is used?

#### For write
When the so_fputc, so_fwrite function is called, the information is
written in buffer. When the buffer fills up, the information is passed
in the file, using the write call. This improves performance,
because we have fewer system calls and less overhead.
The flush_buffer function is used to empty the buffer
The buffer may be emptied before reaching its maximum size
in the following situations:
	1. When closing the file, in which case we write the information we have left
	2. When changing the cursor position with fseek. We will empty the buffer at the current cursor position, and only then will we move the cursor

#### For read
When so_fgetc, so_fread functions are called
information is read from buffer. When the needed information is not found in the
buffer, the next BUF_SIZE bytes from the file are brought to the buffer, or how many bytes are left
to the end of the file. When fseek is called and the cursor is moved,
the information in the buffer is invalidated, being necessary to bring new bytesof 
information to the buffer. To bring information to the buffer, the 
function bring_to_buffer is used.

## How to compile the code?
In a terminal, run:
```
	make build
```








