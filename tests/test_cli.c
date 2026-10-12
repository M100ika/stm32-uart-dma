#include <string.h>

#include "cli.h"
#include "test.h"

/* Весь вывод CLI собираем в строку, чтобы сравнивать с ожидаемым. */
static char s_out[1024];

static void capture(const char *s)
{
    strncat(s_out, s, sizeof(s_out) - strlen(s_out) - 1);
}

static int s_calls;
static int s_argc;
static char s_args[CLI_ARGS_MAX][CLI_LINE_MAX];

static void cmd_spy(cli_t *cli, int argc, char *argv[])
{
    (void)cli;
    s_calls++;
    s_argc = argc;
    for (int i = 0; i < argc; i++) {
        strcpy(s_args[i], argv[i]);
    }
}

static void cmd_hello(cli_t *cli, int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_puts(cli, "hi\r\n");
}

static const cli_cmd_t CMDS[] = {
    { "spy", "records arguments", cmd_spy },
    { "hello", "prints hi", cmd_hello },
};

static cli_t s_cli;

static void setup(void)
{
    s_out[0] = '\0';
    s_calls = 0;
    s_argc = 0;
    cli_init(&s_cli, CMDS, 2, capture);
}

static void feed(const char *s)
{
    while (*s) {
        cli_feed(&s_cli, *s++);
    }
}

static void runs_command_with_args(void)
{
    setup();
    feed("spy  one two   three\r");
    CHECK_EQ(s_calls, 1);
    CHECK_EQ(s_argc, 4);
    CHECK(strcmp(s_args[0], "spy") == 0);
    CHECK(strcmp(s_args[1], "one") == 0);
    CHECK(strcmp(s_args[3], "three") == 0);
}

static void echoes_input_and_prints_prompt(void)
{
    setup();
    feed("hello\r");
    CHECK(strcmp(s_out, "hello\r\nhi\r\n> ") == 0);
}

static void crlf_runs_once(void)
{
    setup();
    feed("spy\r\n");
    CHECK_EQ(s_calls, 1);
    feed("spy\n");    /* одиночный LF тоже работает */
    CHECK_EQ(s_calls, 2);
    feed("\r\n\r\n"); /* пустые строки ничего не вызывают */
    CHECK_EQ(s_calls, 2);
}

static void backspace_edits_line(void)
{
    setup();
    feed("spx\bz\x7f" "y a\r"); /* набрали spx, стёрли x, z, стёрли z, y */
    CHECK_EQ(s_calls, 1);
    CHECK(strcmp(s_args[0], "spy") == 0);
    CHECK(strcmp(s_args[1], "a") == 0);
    CHECK(strstr(s_out, "\b \b") != NULL);
}

static void backspace_on_empty_line_is_ignored(void)
{
    setup();
    feed("\b\b");
    CHECK(s_out[0] == '\0');
}

static void too_long_line_is_rejected(void)
{
    setup();
    for (unsigned i = 0; i < CLI_LINE_MAX + 10U; i++) {
        cli_feed(&s_cli, 'a');
    }
    feed("\r");
    CHECK(strstr(s_out, "error: line too long") != NULL);
    CHECK_EQ(s_calls, 0);
    feed("spy\r");    /* после ошибки CLI снова работает */
    CHECK_EQ(s_calls, 1);
}

static void too_many_args_is_rejected(void)
{
    setup();
    feed("spy 1 2 3 4 5 6 7 8\r"); /* 9 слов при пределе 8 */
    CHECK_EQ(s_calls, 0);
    CHECK(strstr(s_out, "too many arguments") != NULL);
    setup();
    feed("spy 1 2 3 4 5 6 7\r");   /* ровно 8 — допустимо */
    CHECK_EQ(s_calls, 1);
    CHECK_EQ(s_argc, 8);
}

static void unknown_command(void)
{
    setup();
    feed("nope\r");
    CHECK(strstr(s_out, "unknown command: nope") != NULL);
}

static void help_lists_commands(void)
{
    setup();
    feed("help\r");
    CHECK(strstr(s_out, "spy - records arguments") != NULL);
    CHECK(strstr(s_out, "hello - prints hi") != NULL);
}

static void control_chars_ignored(void)
{
    setup();
    feed("sp\x01\x1by\r");
    CHECK_EQ(s_calls, 1);
    CHECK(strcmp(s_args[0], "spy") == 0);
}

static void redraw_restores_partial_line(void)
{
    setup();
    feed("spy ab");
    s_out[0] = '\0';
    cli_redraw(&s_cli);
    CHECK(strcmp(s_out, "> spy ab") == 0);
}

void run_cli_tests(void)
{
    RUN(runs_command_with_args);
    RUN(echoes_input_and_prints_prompt);
    RUN(crlf_runs_once);
    RUN(backspace_edits_line);
    RUN(backspace_on_empty_line_is_ignored);
    RUN(too_long_line_is_rejected);
    RUN(too_many_args_is_rejected);
    RUN(unknown_command);
    RUN(help_lists_commands);
    RUN(control_chars_ignored);
    RUN(redraw_restores_partial_line);
}
