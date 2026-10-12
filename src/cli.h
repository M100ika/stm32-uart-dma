/*
 * cli.h — простая командная строка поверх потока символов.
 *
 * Модуль не знает про UART: символы приходят через cli_feed(), вывод уходит
 * через функцию write, которую передаёт приложение. Поэтому CLI целиком
 * тестируется на ПК.
 *
 * Поддерживается: эхо, Backspace (0x08 и 0x7F), окончания строк CR, LF
 * и CRLF, разбиение на аргументы по пробелам, встроенная команда help.
 */
#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stddef.h>

#define CLI_LINE_MAX 64U  /* включая '\0' */
#define CLI_ARGS_MAX 8U

typedef struct cli cli_t;

typedef void (*cli_write_fn)(const char *s);
typedef void (*cli_handler_fn)(cli_t *cli, int argc, char *argv[]);

typedef struct {
    const char *name;
    const char *help;
    cli_handler_fn handler;
} cli_cmd_t;

struct cli {
    const cli_cmd_t *cmds;
    size_t ncmds;
    cli_write_fn write;
    char line[CLI_LINE_MAX];
    size_t len;
    bool overflow;   /* строка длиннее буфера: выполнять её нельзя */
    char prev;       /* предыдущий символ, чтобы склеить CR+LF */
};

void cli_init(cli_t *cli, const cli_cmd_t *cmds, size_t ncmds, cli_write_fn write);

/* Обработать один принятый символ. Вызывать из основного цикла. */
void cli_feed(cli_t *cli, char c);

/* Вывод для обработчиков команд. */
void cli_puts(cli_t *cli, const char *s);

void cli_prompt(cli_t *cli);

/* Заново вывести приглашение и набранную часть строки — после того как
 * приложение напечатало что-то асинхронное (например, событие кнопки). */
void cli_redraw(cli_t *cli);

#endif /* CLI_H */
