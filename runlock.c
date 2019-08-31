/*
 * Copyright (c) 2019, Michael Santos <michael.santos@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "runlock.h"

#define RUNLOCK_VERSION "0.3.0"

#define VERBOSE(__n, ...)                                                      \
  do {                                                                         \
    if (verbose >= __n) {                                                      \
      (void)fprintf(stderr, __VA_ARGS__);                                      \
    }                                                                          \
  } while (0)

extern char *__progname;

enum {
  OPT_DRYRUN = 1,
  OPT_PRINT = 2,
};

static const struct option long_options[] = {
    {"timestamp", required_argument, NULL, 't'},
    {"file", required_argument, NULL, 'f'},
    {"timeout", required_argument, NULL, 'T'},
    {"signal", required_argument, NULL, 's'},
    {"dryrun", no_argument, NULL, 'n'},
    {"print", no_argument, NULL, 'P'},
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

int waitfor(int *status);
int signal_init();
void signal_handler(int sig);
static void usage();

pid_t pid;
int default_signal = SIGTERM;

int main(int argc, char *argv[]) {
  int fd;
  const char *file = "runlock.lock";
  u_int32_t timestamp = 0;
  int32_t timeout = 0;
  int verbose = 0;
  int opt = 0;
  int ch;
  const char *errstr = NULL;

  struct stat buf;
  int status = 0;
  int exit_value = 0;

  if (setvbuf(stdout, NULL, _IOLBF, 0) < 0)
    err(EXIT_FAILURE, "setvbuf");

  while ((ch = getopt_long(argc, argv, "f:hnPvs:t:T:", long_options, NULL)) !=
         -1) {
    switch (ch) {
    case 'f':
      file = optarg;
      break;

    case 's':
      default_signal = strtonum(optarg, 0, NSIG - 1, &errstr);
      if (errno)
        err(EXIT_FAILURE, "strtonum: %s: %s", optarg, errstr);
      break;

    case 't':
      timestamp = strtonum(optarg, 1, UINT_MAX, &errstr);
      if (errno)
        err(EXIT_FAILURE, "strtonum: %s: %s", optarg, errstr);
      break;

    case 'T':
      timeout = strtonum(optarg, INT_MIN, INT_MAX, &errstr);
      if (errno)
        err(EXIT_FAILURE, "strtonum: %s: %s", optarg, errstr);
      break;

    case 'n':
      opt |= OPT_DRYRUN;
      break;

    case 'P':
      opt |= OPT_PRINT;
      break;

    case 'v':
      verbose++;
      break;

    case 'h':
    default:
      usage();
    }
  }

  argc -= optind;
  argv += optind;

  if (argc == 0)
    usage();

  fd = open(file, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);

  if (fd < 0) {
    switch (errno) {
    case EEXIST:
      fd = open(file, O_RDWR | O_CLOEXEC, 0);
      if (fd < 0)
        err(111, "open");
      break;
    default:
      err(111, "open");
    }
  } else {
    struct timeval tv[2] = {0};

    tv[0].tv_sec = timestamp;
    tv[1].tv_sec = timestamp;

    if (futimes(fd, tv) < 0)
      err(111, "futimes");
  }

  if (!(opt & OPT_DRYRUN) && (flock(fd, LOCK_EX | LOCK_NB) < 0))
    exit(111);

  if (fstat(fd, &buf) < 0)
    err(111, "fstat");

  VERBOSE(2, "timestamp:%u\nfile:%lu\n", timestamp, buf.st_mtim.tv_sec);

  if (opt & OPT_PRINT)
    (void)printf("%.f\n", difftime(timestamp, buf.st_mtim.tv_sec));

  if (timestamp < buf.st_mtim.tv_sec) {
    exit(121);
  }

  if (opt & OPT_DRYRUN)
    exit(0);

  pid = fork();

  switch (pid) {
  case -1:
    exit(111);
  case 0:
    if (setpgid(0, 0) < 0)
      err(111, "setpgid");

    (void)execvp(argv[0], argv);
    exit(111);
  default:
    if (signal_init() < 0) {
      (void)kill(pid * -1, default_signal);
    }
    if (timeout == 0) {
      timeout = difftime(time(NULL), timestamp);
    }
    if (timeout > 0) {
      VERBOSE(1, "command timeout set to %ds\n", timeout);
      alarm(timeout);
    }
    if (waitfor(&status) < 0) {
      exit_value = 111;
      goto RUNLOCK_EXIT;
    }
  }

  if (WIFEXITED(status))
    exit_value = WEXITSTATUS(status);
  else if (WIFSIGNALED(status))
    exit_value = 128 + WTERMSIG(status);

RUNLOCK_EXIT:
  VERBOSE(3, "status=%d exit_value=%d\n", status, exit_value);

  switch (exit_value) {
  case 0:
    /* update the timestamp */
    if (ftruncate(fd, 0) < 0)
      exit(111);

    break;
  default:
    break;
  }

  exit(exit_value);
}

int waitfor(int *status) {
  for (;;) {
    errno = 0;
    if (wait(status) < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    return 0;
  }
}

void signal_handler(int sig) {
  if (pid > 0)
    (void)kill(pid * -1, sig == SIGALRM ? default_signal : sig);
}

int signal_init() {
  struct sigaction act = {0};
  int sig = 0;

  act.sa_handler = signal_handler;
  (void)sigfillset(&act.sa_mask);

  for (sig = 1; sig < NSIG; sig++) {
    if (sig == SIGCHLD)
      continue;

    if (sigaction(sig, &act, NULL) < 0) {
      if (errno == EINVAL)
        continue;

      return -1;
    }
  }

  return 0;
}

static void usage() {
  errx(EXIT_FAILURE,
       "[OPTION] <PATH>\n"
       "version: %s\n\n"
       "-t, --timestamp <seconds> threshold timestamp\n"
       "-T, --timeout <seconds>   command timeout\n"
       "-s, --signal <sig>        signal sent on timeout (default: SIGTERM)\n"
       "-n, --dryrun              do nothing\n"
       "-P, --print               print remaing time until next run\n"
       "-f, --file                timestamp lock file\n"
       "-v, --verbose             verbose mode\n",
       RUNLOCK_VERSION);
}
