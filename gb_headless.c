#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gb.h"

typedef enum CommandType {
    CT_INVALID = 0,
    CT_HELP,
    CT_QUIT,
    CT_INFO,
    CT_STEP,
    //CT_NEXT,
    CT_BREAK,
    CT_LIST,
    //CT_PRINT,
    CT_EXAMINE,
    CT_CONTINUE,
    CT_COUNT
} CommandType;

typedef struct Command {
    CommandType type;
    size_t addr;
} Command;

#define MAX_BREAKPOINTS 16
static u16 breakpoints[MAX_BREAKPOINTS];
static size_t bp_count;

static char* trim_cstr(char *s)
{
    char *start = s;
    char *end = s + strlen(s) - 1;

    while (isspace(*start)) {
        start++;
    }

    while (end > start && isspace(*end)) {
        *end = '\0';
        end--;
    }

    return start;
}

Command read_command(void)
{
    printf("> ");
    char buffer[128];
    char *res = fgets(buffer, sizeof(buffer), stdin);
    if (!res) exit(1);
    char *s = trim_cstr(buffer);
    size_t len = strlen(s);

    Command cmd = {0};
    if (strncasecmp(s, "quit", len) == 0) cmd.type = CT_QUIT;
    else if (strncasecmp(s, "q", 1) == 0) cmd.type = CT_QUIT;
    else if (strncasecmp(s, "help", len) == 0) cmd.type = CT_HELP;
    else if (strncasecmp(s, "h", 1) == 0) cmd.type = CT_HELP;
    else if (strncasecmp(s, "info", len) == 0) cmd.type = CT_INFO;
    else if (strncasecmp(s, "i", 1) == 0) cmd.type = CT_INFO;
    else if (strncasecmp(s, "step", len) == 0) cmd.type = CT_STEP;
    else if (strncasecmp(s, "s", 1) == 0) cmd.type = CT_STEP;
    else if (strncasecmp(s, "list", len) == 0) cmd.type = CT_LIST;
    else if (strncasecmp(s, "l", 1) == 0) cmd.type = CT_LIST;
    else if (strncasecmp(s, "continue", len) == 0) cmd.type = CT_CONTINUE;
    else if (strncasecmp(s, "c", 1) == 0) cmd.type = CT_CONTINUE;
    else if (strncasecmp(s, "break", 5) == 0) cmd.type = CT_BREAK;
    else if (strncasecmp(s, "b", 1) == 0) {
        cmd.type = CT_BREAK;
        char *addr = trim_cstr(s + 2);
        cmd.addr = (size_t)strtoul(addr, 0, 16);
    }
    else if (strncasecmp(s, "examine", 7) == 0) cmd.type = CT_EXAMINE;
    else if (strncasecmp(s, "x", 1) == 0) {
        cmd.type = CT_EXAMINE;
        char *addr = trim_cstr(s + 2);
        cmd.addr = (size_t)strtoul(addr, 0, 16);
    }

    return cmd;
}

void cmd_help(void)
{
    printf("Commands:\n");
    printf("  quit (q):     Quit the application\n");
    printf("  help (h):     Show this help message\n");
    printf("  step (s):     Execute the current line (step into functions)\n");
    printf("\n");
}

void cmd_info(const GameBoy *gb)
{
    printf("AF: $%04x    A:  $%02x    F: %c%c%c%c\n",
        gb->AF, gb->A,
        (gb->F & 0x80) ? 'Z' : '-',
        (gb->F & 0x40) ? 'N' : '-',
        (gb->F & 0x20) ? 'H' : '-',
        (gb->F & 0x10) ? 'C' : '-');
    printf("BC: $%04x    B:  $%02x    C: $%02x\n", gb->BC, gb->B, gb->C);
    printf("DE: $%04x    D:  $%02x    E: $%02x\n", gb->DE, gb->D, gb->E);
    printf("HL: $%04x    H:  $%02x    L: $%02x\n", gb->HL, gb->H, gb->L);
    printf("PC: $%04x    SP: $%04x\n", gb->PC, gb->SP);
}

void cmd_step(GameBoy *gb)
{
    Inst inst = gb_fetch(gb);

    printf("$%04x: ", gb->PC);
    if (inst.size == 1) printf("%02x           ", inst.data[0]);
    if (inst.size == 2) printf("%02x %02x        ", inst.data[0], inst.data[1]);
    if (inst.size == 3) printf("%02x %02x %02x     ", inst.data[0], inst.data[1], inst.data[2]);
    char buf[32] = {0};
    printf("%s\n", gb_decode(inst, buf, sizeof(buf)));

    int cycles = gb_exec(gb, inst);
    (void)cycles;
    //printf("Cycles: %d\n", cycles);
}

void cmd_list(GameBoy *gb)
{
    int old_pc = gb->PC;
    for (int i = 0; i < 10; i++) {
        Inst inst = gb_fetch(gb);
        printf("$%04x: ", gb->PC);
        gb->PC += inst.size;
        if (inst.size == 1) printf("%02x           ", inst.data[0]);
        if (inst.size == 2) printf("%02x %02x        ", inst.data[0], inst.data[1]);
        if (inst.size == 3) printf("%02x %02x %02x     ", inst.data[0], inst.data[1], inst.data[2]);
        char buf[32] = {0};
        printf("%s\n", gb_decode(inst, buf, sizeof(buf)));
    }
    gb->PC = old_pc;
}

void cmd_examine(GameBoy *gb, Command cmd)
{
    assert(cmd.type == CT_EXAMINE);
    u8 value = gb->memory[cmd.addr];
    printf("$%04x: %02x (%3d)\n", (u16)cmd.addr, value, value);
}

void cmd_break(GameBoy *gb, Command cmd)
{
    (void)gb;
    assert(cmd.type == CT_BREAK);
    assert(bp_count < MAX_BREAKPOINTS);
    breakpoints[bp_count++] = cmd.addr;
    printf("Setting breakpoint at $%04x\n", (u16)cmd.addr);
}

int main(int argc, char **argv)
{
    GameBoy gb = {0};
    gb_init_with_args(&gb, argc, argv);

    bool running = true;
    bool single_stepping = true;
    while (running) {
        if (single_stepping) {
            Command cmd = read_command();
            switch (cmd.type) {
                case CT_QUIT: running = false; break;
                case CT_HELP: cmd_help(); break;
                case CT_INFO: cmd_info(&gb); break;
                case CT_STEP: cmd_step(&gb); break;
                case CT_LIST: cmd_list(&gb); break;
                case CT_CONTINUE: {
                    single_stepping = false;
                    Inst inst = gb_fetch(&gb);
                    gb_exec(&gb, inst);
                } break;
                case CT_BREAK: cmd_break(&gb, cmd); break;
                case CT_EXAMINE: cmd_examine(&gb, cmd); break;
                default: printf("Unhandled command\n");
            }
        } else {
            for (size_t i = 0; i < bp_count; i++) {
                if (breakpoints[i] == gb.PC) {
                    printf("Hit breakpoint at $%04x\n", gb.PC);
                    single_stepping = true;
                    break;
                }
            }
            if (single_stepping) continue;

            //Inst inst = gb_fetch(&gb);
            //gb_exec(&gb, inst);
            //printf("$%04x (gb_headless)\n", gb.PC);
            gb_update(&gb);
        }
    }

    return 0;
}
