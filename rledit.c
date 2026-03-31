#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/readline.h>

static int chain = 0;
static char *text;

static int prefill(void) {
  rl_insert_text(text);
  return 0;
}

static char *slurp(int fd) {
  ssize_t count, length = 0, size = 0;
  char *buffer = NULL;

  do {
    if (length >= size)
      if (!(buffer = realloc(buffer, size = size ? size << 1 : 65536)))
        err(1, "realloc");
    if ((count = read(fd, buffer + length, size - length)) > 0)
      length += count;
    if (count < 0 && errno != EAGAIN && errno != EINTR)
      err(1, "read");
  } while (count);

  if (length && buffer[length - 1] == '\n')
    buffer[--length] = 0;
  return buffer;
}

static int visual(int count, int key) {
  rl_clear_visible_line();
  chain = rl_done = 1;
  return 0;
}

int main(int argc, char **argv) {
  int in, out, tty;

  switch (argc) {
    case 1:
      if ((in = dup(0)) < 0 || (out = dup(1)) < 0)
        err(1, "dup");
      break;
    case 2:
      if ((in = out = open(argv[1], O_RDWR | O_CREAT, 0666)) < 0)
        err(1, argv[1]);
      break;
    default:
      fprintf(stderr, "Usage: %s [FILE]\n", argv[0]);
      return 64;
  }

  if ((tty = open("/dev/tty", O_RDWR)) < 0)
    err(1, "/dev/tty");
  if (dup2(tty, 0) < 0 || dup2(tty, 1) < 0)
    err(1, "dup2");
  close(tty);

  if (argc > 1)
    rl_bind_keyseq_in_map("\\C-x\\C-e", visual, emacs_standard_keymap);

  rl_macro_bind("\\C-j", "\\C-v\\C-j", emacs_standard_keymap);
  rl_macro_bind("\\e\\C-m", "\\C-v\\C-j", emacs_standard_keymap);
  rl_macro_bind("\\e[27;2;13~", "\\C-v\\C-j", emacs_standard_keymap);
  rl_macro_bind("\\e[27;5;13~", "\\C-v\\C-j", emacs_standard_keymap);
  rl_inhibit_completion = 1;
  rl_startup_hook = prefill;

  text = slurp(in);
  if (in != out)
    close(in);

  dprintf(1, "\033[3m"); /* italic style */
  text = readline("");
  dprintf(1, "\033[0m"); /* default style */

  if (argc > 1) {
    lseek(out, 0, SEEK_SET);
    ftruncate(out, 0);
  }

  if (text && *text && dprintf(out, "%s\n", text) < 0)
    err(1, argv[1]);
  close(out);

  if (chain && argc > 1)
    if (execlp("sh", "sh", "-c", "${VISUAL:-${EDITOR:-vi}} \"$1\"",
        argv[0], argv[1], NULL) < 0)
      err(1, "execlp");
  return 0;
}
