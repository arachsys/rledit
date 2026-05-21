#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/readline.h>

static int external;
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
  external = rl_done = 1;
  return 0;
}

int main(int argc, char **argv) {
  int fd, tty;

  setlocale(LC_CTYPE, "");

  rl_macro_bind("\\C-j", "\\C-v\\C-j", emacs_standard_keymap);
  rl_macro_bind("\\e\\C-m", "\\C-v\\C-j", emacs_standard_keymap);
  rl_macro_bind("\\e[27;2;13~", "\\C-v\\C-j", emacs_standard_keymap);
  rl_macro_bind("\\e[27;5;13~", "\\C-v\\C-j", emacs_standard_keymap);
  rl_inhibit_completion = 1;
  rl_startup_hook = prefill;

  if (argc == 2 && isatty(0) && isatty(1)) {
    rl_bind_keyseq_in_map("\\C-x\\C-e", visual, emacs_standard_keymap);

    if (setenv("FILE", argv[1], 1))
      err(1, "setenv");

    while (1) {
      if ((fd = open(argv[1], O_RDWR | O_CREAT, 0666)) < 0)
        err(1, argv[1]);
      text = slurp(fd);
      external = 0;

      dprintf(1, "\033[3m\033[1 q"); /* italic style, blinking cursor */
      text = readline("");
      dprintf(1, "\033[0m\033[0 q"); /* default style, default cursor */

      lseek(fd, 0, SEEK_SET);
      ftruncate(fd, 0);
      if (text && *text && dprintf(fd, "%s\n", text) < 0)
        err(1, argv[1]);
      close(fd);

      if (external == 0)
        return 0;
      if (system("${VISUAL:-${EDITOR:-vi}} \"$FILE\""))
        return 1;
    }
  }

  if (argc == 1 && (!isatty(0) || !isatty(1))) {
    if (fcntl(0, F_GETFD) < 0)
      errx(1, "/dev/stdin is not open");
    if (fcntl(1, F_GETFD) < 0)
      errx(1, "/dev/stdout is not open");

    if ((fd = dup(1)) < 0)
      err(1, "dup");
    text = slurp(0);

    if ((tty = open("/dev/tty", O_RDWR)) < 0)
      err(1, "/dev/tty");
    if (dup2(tty, 0) < 0 || dup2(tty, 1) < 0)
      err(1, "dup2");
    close(tty);

    dprintf(1, "\033[3m\033[1 q"); /* italic style, blinking cursor */
    text = readline("");
    dprintf(1, "\033[0m\033[0 q"); /* default style, default cursor */

    if (text && *text && dprintf(fd, "%s\n", text) < 0)
      err(1, argv[1]);
    close(fd);
    return 0;
  }

  fprintf(stderr, "Usage: %s [FILE]\n", argv[0]);
  return 64;
}
