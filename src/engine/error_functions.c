/*
 * Copyright 2012 Vaibhav Bajpai <contact@vaibhavbajpai.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdarg.h>

#include "error_functions.h"
#include "base_header.h"

#ifdef __GNUC__
  __attribute__ ((__noreturn__)) 
#endif

static void
terminate() {

  /* Dump core if EF_DUMPCORE environment variable is defined
   * and is a nonempty string; otherwise call exit(3)
   */
  char *s;
  s = getenv("EF_DUMPCORE");
  if (s != NULL && *s != '\0')
    abort();

  exit(EXIT_FAILURE);
}

static void
outputError(int err, Boolean flushStdout,
            const char *format, va_list ap) {

  #define BUF_SIZE 500
  char buf[BUF_SIZE], userMsg[BUF_SIZE], errText[BUF_SIZE];
  
  vsnprintf(userMsg, BUF_SIZE, format, ap);  
  snprintf(errText, BUF_SIZE, ":");  
  snprintf(buf, BUF_SIZE, "ERROR%s %s\n", errText, userMsg);
  
  if (flushStdout)
    fflush(stdout); /* Flush any pending stdout */
  fputs(buf, stderr);
  fflush(stderr);
}

void 
usageErr(const char *format, ...) {
  
  va_list argList;
  fflush(stdout); /* Flush any pending stdout */
  
  fprintf(stderr, "Usage: "); 
  va_start(argList, format); 
  vfprintf(stderr, format, argList); 
  va_end(argList);
  
  fflush(stderr); /* In case stderr is not line-buffered */
  exit(EXIT_FAILURE);
}

void
errExit(const char *format, ...) {
  
  va_list argList;
  
  va_start(argList, format);
  outputError(errno, TRUE, format, argList); 
  va_end(argList);
  
  terminate();
}