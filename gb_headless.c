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
    bool single_stepping = argc != 2;
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

static u64 t_cycles;
static u64 m_cycles;

static u16 fetch_addr;
static Inst fetch_inst;

void gb_tick_m(GameBoy *gb);

int main2(void)
{
    GameBoy gb = {0};

    // Fetch $0000 (00        NOP)
    gb_tick_m(&gb);
    assert(m_cycles == 1 && t_cycles == 4);
    assert(fetch_addr == 0x0001);

    gb_tick_m(&gb);
    assert(m_cycles == 2 && t_cycles == 8);

    // Execute NOP
    // Fetch $0001 (00        NOP)

    printf("Pass\n");

    return 0;
}

void gb_tick_m(GameBoy *gb)
{
    t_cycles += 4;
    m_cycles += 1;

    // Execute
    if (fetch_inst.size) {
        // 1 M-cycle
        // NOP | STOP
        // INC B | INC C | INC D | INC E | INC H | INC L | INC A
        // DEC B | DEC C | DEC D | DEC E | DEC H | DEC L | DEC A
        // RLCA | RRCA | RLA | RRA | DAA | CPL | SCF | CCF
        // LD B,B | LD B,C | LD B,D | LD B,E | LD B,H | LD B,L | LD B,A
        // LD C,B | ...                                        | LD C,A
        // LD D,B | ...                                        | LD D,A
        // LD E,B | ...                                        | LD E,A
        // LD H,B | ...                                        | LD H,A
        // LD L,B | ...                                        | LD L,A
        // LD A,B | ...                                        | LD A,A
        // HALT
        // ADD B | ADD C | ADD D | ADD E | ADD H | ADD L | ADD A
        // ADC B | ...                                   | ADC A
        // SUB B | ...                                   | SUB A
        // SBC B | ...                                   | SBC A
        // AND B | ...                                   | AND A
        // XOR B | ...                                   | XOR A
        // OR  B | ...                                   | OR  A
        // CP  B | ...                                   | CP  A
        // JP (HL)
        // DI | EI
        if (fetch_inst.cycles == 4) {
        }

        // 2 M-cycles
        // JR  cc (not taken)
        // RET cc (not taken)
        // LD (BC),A | LD (DE),A | LD (HL+),A | LD (HL-),A
        // LD A,(BC) | LD A,(DE) | LD A,(HL+) | LD A,(HL-)
        // INC BC | INC DE | INC HL | INC SP
        // DEC BC | DEC DE | DEC HL | DEC SP
        // LD B,d8 | LD C,d8 | LD D,d8 | LD E,d8 | LD H,d8 | LD L,d8 | LD A,d8
        // ADD HL,BC | ADD HL,DE | ADD HL,HL | ADD HL,SP
        // LD B,(HL) | LD C,(HL) | LD D,(HL) | LD E,(HL) | LD H,(HL) | LD L,(HL) | LD A,(HL)
        // ADD (HL) | ADC (HL) | SUB (HL) | SBC (HL) | AND (HL) | XOR (HL) | OR (HL) | CP (HL)
        // ADD d8 | ADC d8 | SUB d8 | SBC d8 | AND d8 | XOR d8 | OR d8 | CP d8
        // LD (C),A | LD A,(C)
        // LD SP,HL
        // RLC  B | ... | RLC  A | RRC  B | ... | RRC A
        // RL   B | ... | RL   A | RR   B | ... | RR  A
        // SLA  B | ... | SLA  A | SRA  B | ... | SRA A
        // SWAP B | ... | SWAP A | SRL  B | ... | SRL A
        // BIT 0,B| ... | BIT 0,A| BIT 1,B| ... | BIT 1,A
        // BIT 2,B| ... | BIT 2,A| BIT 3,B| ... | BIT 3,A
        // BIT 4,B| ... | BIT 4,A| BIT 5,B| ... | BIT 5,A
        // BIT 6,B| ... | BIT 6,A| BIT 7,B| ... | BIT 7,A
        // RES 0,B| ... | RES 0,A| RES 1,B| ... | RES 1,A
        // RES 2,B| ... | RES 2,A| RES 3,B| ... | RES 3,A
        // RES 4,B| ... | RES 4,A| RES 5,B| ... | RES 5,A
        // RES 6,B| ... | RES 6,A| RES 7,B| ... | RES 7,A
        // SET 0,B| ... | SET 0,A| SET 1,B| ... | SET 1,A
        // SET 2,B| ... | SET 2,A| SET 3,B| ... | SET 3,A
        // SET 4,B| ... | SET 4,A| SET 5,B| ... | SET 5,A
        // SET 6,B| ... | SET 6,A| SET 7,B| ... | SET 7,A

        // 3 M-cycles
        // JR r8
        // JR cc (taken)
        // JP cc,a16 (not taken)
        // CALL cc,a16 (not taken)
        // LD BC,d16 | LD DE,d16 | LD HL,d16 | LD SP,d16
        // INC (HL) | DEC (HL)
        // LD (HL),d8
        // POP BC | POP DE | POP HL | POP AF
        // LDH (a8),A | LDH A,(a8)
        // LD HL,SP+r8

        // 4 M-cycles
        // JP a16
        // JP cc,a16 (taken)
        // PUSH BC | PUSH DE | PUSH HL | PUSH AF
        // RST $00 | RST $08 | RST $10 | RST $18 | RST $20 | RST $28 | RST $30 | RST $38
        // RET | RETI
        // ADD SP,r8
        // LD (a16),A | LD A,(a16)
        // RLC (HL) | RRC (HL) | RL (HL) | RR (HL) | SLA (HL) | SRA (HL) | SWAP (HL) | SRL (HL)
        // BIT 0,(HL) | ... | BIT 7,(HL) | RES 0,(HL) | ... | RES 7,(HL) | SET 0,(HL) | ... | SET 7,(HL)

        // 5 M-cycles
        // LD (a16), SP | RET cc (taken)

        // 6 M-cycles
        // CALL a16
        // CALL cc,a16 (taken)
    }

    // Fetch
    fetch_inst = gb_fetch_internal(gb->memory + fetch_addr, gb->F, false);
    fetch_addr += fetch_inst.size;
}
