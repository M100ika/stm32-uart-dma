#include "cli.h"

#include <string.h>

static const char PROMPT[] = "> ";

void cli_init(cli_t *cli, const cli_cmd_t *cmds, size_t ncmds, cli_write_fn write)
{
    memset(cli, 0, sizeof(*cli));
    cli->cmds = cmds;
    cli->ncmds = ncmds;
    cli->write = write;
}

void cli_puts(cli_t *cli, const char *s)
{
    cli->write(s);
}

void cli_prompt(cli_t *cli)
{
    cli_puts(cli, PROMPT);
}

void cli_redraw(cli_t *cli)
{
    cli_prompt(cli);
    if (!cli->overflow && cli->len > 0U) {
        cli->line[cli->len] = '\0';
        cli_puts(cli, cli->line);
    }
}

static void print_help(cli_t *cli)
{
    cli_puts(cli, "commands:\r\n  help - this list\r\n");
    for (size_t i = 0; i < cli->ncmds; i++) {
        cli_puts(cli, "  ");
        cli_puts(cli, cli->cmds[i].name);
        cli_puts(cli, " - ");
        cli_puts(cli, cli->cmds[i].help);
        cli_puts(cli, "\r\n");
    }
}

/* Разбивает строку на слова на месте, заменяя пробелы на '\0'.
 * Возвращает число слов или -1, если их больше CLI_ARGS_MAX. */
static int tokenize(char *line, char *argv[CLI_ARGS_MAX])
{
    int argc = 0;
    char *p = line;
    for (;;) {
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0') {
            return argc;
        }
        if (argc == (int)CLI_ARGS_MAX) {
            return -1;
        }
        argv[argc++] = p;
        while (*p != ' ' && *p != '\0') {
            p++;
        }
        if (*p == ' ') {
            *p++ = '\0';
        }
    }
}

static void execute(cli_t *cli)
{
    if (cli->overflow) {
        cli_puts(cli, "error: line too long\r\n");
        return;
    }
    cli->line[cli->len] = '\0';

    char *argv[CLI_ARGS_MAX];
    int argc = tokenize(cli->line, argv);
    if (argc < 0) {
        cli_puts(cli, "error: too many arguments\r\n");
        return;
    }
    if (argc == 0) {
        return;
    }
    if (strcmp(argv[0], "help") == 0) {
        print_help(cli);
        return;
    }
    for (size_t i = 0; i < cli->ncmds; i++) {
        if (strcmp(argv[0], cli->cmds[i].name) == 0) {
            cli->cmds[i].handler(cli, argc, argv);
            return;
        }
    }
    cli_puts(cli, "unknown command: ");
    cli_puts(cli, argv[0]);
    cli_puts(cli, " (type help)\r\n");
}

void cli_feed(cli_t *cli, char c)
{
    const char prev = cli->prev;
    cli->prev = c;

    if (c == '\r' || c == '\n') {
        if (c == '\n' && prev == '\r') {
            return; /* вторая половина CRLF: строка уже выполнена */
        }
        cli_puts(cli, "\r\n");
        execute(cli);
        cli->len = 0;
        cli->overflow = false;
        cli_prompt(cli);
        return;
    }

    if (c == '\b' || c == 0x7F) {
        if (cli->len > 0U && !cli->overflow) {
            cli->len--;
            cli_puts(cli, "\b \b"); /* стереть символ в терминале */
        }
        return;
    }

    if (c < 0x20 || c > 0x7E) {
        return; /* прочие управляющие символы и не-ASCII игнорируем */
    }

    if (cli->len < CLI_LINE_MAX - 1U) {
        cli->line[cli->len++] = c;
        const char echo[2] = { c, '\0' };
        cli_puts(cli, echo);
    } else {
        cli->overflow = true;
    }
}
