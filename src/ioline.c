/*
 *      Wapiti - A linear-chain CRF tool
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ioline.h"
#include "tools.h"

static char *iol_gets(void *in);
static int   iol_print(void *out, char *msg, ...);
static int   iol_sprint(void *out, char *msg, ...);
static char *iol_gets_cb_interop(void *in);

iol_t *iol_new(FILE *in, FILE *out) {
    iol_t *iol = xmalloc(sizeof(iol_t));
    iol->gets_cb  = iol_gets,
    iol->in       = in;
    iol->print_cb = iol_print;
    iol->write_cb = NULL;
    iol->out      = out;
    return iol;
}

iol_t *iol_new2(gets_cb_t gets_cb, void *in, print_cb_t print_cb, void *out) {
    iol_t *iol = xmalloc(sizeof(iol_t));
    iol->gets_cb  = gets_cb;
    iol->in       = in;
    iol->print_cb = print_cb;
    iol->write_cb = NULL;
    iol->out      = out;
    return iol;
}

iol_t *iol_new_interop(gets_cb_t gets_cb, write_cb_t write_cb) {
    iol_t *iol = xmalloc(sizeof(iol_t));

	iol_interop_t *iol_interop = xmalloc(sizeof(iol_interop_t));
	iol_interop->gets_cb = gets_cb;
	iol_interop->in = (void *)iol;

	iol->gets_cb = iol_gets_cb_interop;
	iol->in = (void *)iol_interop;
    iol->print_cb = iol_sprint;
    iol->write_cb = write_cb;
    iol->out      = (void *)iol;
    return iol;
}

void iol_free(iol_t *iol) {
    xfree(iol);
}

void iol_free_interop(iol_t *iol) {
	iol_interop_t *iol_interop = (iol_interop_t*)iol->in;
	xfree(iol_interop);
	xfree(iol);
}

/* gets_cb_interop:
 *  Gets a line of input for interop code.  The caller most likely owns the
 *  memory, so duplicate the string before processing.
 */
static char *iol_gets_cb_interop(void *s) {
	iol_interop_t *iol_interop = (iol_interop_t*)s;
	char *p = iol_interop->gets_cb(iol_interop->in);
	if (p == NULL) {
		return NULL;
	}

	char *line = xstrdup(p);
	return line;
}

static char *print_cb_interop(void *s) {
	return NULL;
}

static char *write_cb_interop(void *s) {
	return NULL;
}

/* iol_gets:
 *   Read an input line from <in>. The line can be of any size limited only by
 *   available memory, a buffer large enough is allocated and returned. The
 *   caller is responsible to free it. If the input is exhausted, NULL is returned.
 */
static char *iol_gets(void *in) {
	FILE *file = (FILE*)in;
	// Initialize the buffer
	uint32_t len = 0, size = 16;
	char *buffer = xmalloc(size);
	// We read the line chunk by chunk until end of line, file or error
	for(;;) {
		if (fgets(buffer + len, size - len, file) == NULL) {
			// On NULL return there is two possible cases, either an
			// error or the end of file
			if (ferror(file))
				pfatal("cannot read from file");
			// On end of file, we must check if we have already read
			// some data or not
			if (len == 0) {
				xfree(buffer);
				return NULL;
			}
			break;
		}
		// Check for end of line, if this is not the case enlarge the
		// buffer and go read more data
		len += strlen(buffer + len);
		if (len == size - 1 && buffer[len - 1] != '\n') {
			size = size * 1.4;
			buffer = xrealloc(buffer, size);
			continue;
		}
		break;
	}
	// At this point empty line should have already catched so we just
	// remove the end of line if present and resize the buffer to fit the
	// data
	if (buffer[len - 1] == '\n')
		buffer[--len] = '\0';
	return xrealloc(buffer, len + 1);
}

/* iol_print:
 *   Print a line to <out>.
 */
static int iol_print(void *out, char *msg, ...) {
	FILE *file = (FILE*)out;
	va_list args;
	va_start(args, msg);
	int rc = vfprintf(file, msg, args);
	va_end(args);
        return rc;
}

/* iol_sprin
     sprintf a line, and forward it to the write_cb_t.
*/
static int iol_sprint(void *out, char *msg, ...) {
    char *p;
    int n = 0;
    size_t size = 128;
    iol_t *iol = (iol_t*)out;
    
    p = xmalloc(size);

    while (1) {
        if (size >= 16384)
            fatal("iol_sprintf");

		va_list args;
		va_start(args, msg);
		n = vsnprintf(p, size, msg, args);
		va_end(args);

        if (n > -1 && (size_t)n < size)
            break;

        size *= 2;
        p = xrealloc(p, size);
    }

    iol->write_cb(p, n);
    return n;
}

