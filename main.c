#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

#define MEMORY_MAX (1 << 16)

// Array representing LC-3's entire memory.'
uint16_t memory[MEMORY_MAX];

uint16_t swap16(uint16_t x);
void read_image_file(FILE* file);
int read_image(const char* image_path);
uint16_t mem_read(uint16_t address);
uint16_t sign_extend(uint16_t x, int bit_count);
void update_flags(uint16_t r);
void mem_write(uint16_t address, uint16_t val);
uint16_t check_key();
void disable_input_buffering();
void restore_input_buffering();

enum
{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,
    R_COND,
    R_COUNT
};

// Special addresses reserved for keyboard interaction.
enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
};

enum
{
    TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
    TRAP_OUT = 0x21,   /* output a character */
    TRAP_PUTS = 0x22,  /* output a word string */
    TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24, /* output a byte string */
    TRAP_HALT = 0x25   /* halt the program */
};

uint16_t reg[R_COUNT];

enum
{
    FL_POS = 1 << 0,
    FL_ZERO = 1 << 1,
    FL_NEG = 1 << 2,
};

enum
{
    OP_BR = 0,
    OP_ADD,
    OP_LD,
    OP_ST,
    OP_JSR,
    OP_AND,
    OP_LDR,
    OP_STR,
    OP_RTI,
    OP_NOT,
    OP_LDI,
    OP_STI,
    OP_JMP,
    OP_RES,
    OP_LEA,
    OP_TRAP
};

struct termios original_tio;

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }

    disable_input_buffering();

    for (int j = 1; j < argc; ++j)
    {
        if (!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }

    reg[R_COND] = FL_ZERO;

    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    int running = 1;

    while (running)
    {
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;

        switch (op)
        {
            case OP_ADD:
            {
                // extracting destination register (DR)
                uint16_t r0 = (instr >> 9) & 0x7;

                // extracting first operand (SR1)
                uint16_t r1 = (instr >> 6) & 0x7;

                // check for immediate mode (imm5)
                uint16_t imm_flag = (instr >> 5) & 0x1;

                if (imm_flag)
                {
                    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                    reg[r0] = reg[r1] + imm5;
                }
                else
                {
                    uint16_t r2 = instr & 0x7;
                    reg[r0] = reg[r1] + reg[r2];
                }

                update_flags(r0);
                break;
            }

            case OP_AND:
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t imm_flag = (instr >> 5) & 0x1;

                if (imm_flag)
                {
                    // immediate mode
                    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                    reg[r0] = reg[r1] & imm5;
                }
                else
                {
                    // register mode
                    uint16_t r2 = instr & 0x7;
                    reg[r0] = reg[r1] & reg[r2];
                }

                update_flags(r0);
                break;
            }

            case OP_NOT:
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;

                reg[r0] = ~reg[r1];

                update_flags(r0);
                break;
            }

            case OP_BR:
            {
                // PCoffset9
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

                // Condition codes
                uint16_t cond_flag = (instr >> 9) & 0x7;

                if (cond_flag & reg[R_COND])
                {
                    reg[R_PC] += pc_offset;
                }

                break;
            }

            case OP_JMP:
            {
                uint16_t r0 = (instr >> 6) & 0x7;

                reg[R_PC] = reg[r0];

                break;
            }

            case OP_JSR:
            {
                uint16_t flag = (instr >> 11) & 1;

                reg[R_R7] = reg[R_PC];

                if (flag) // JSR
                {
                    uint16_t pc_offset = sign_extend(instr & 0x7FF, 11);
                    reg[R_PC] += pc_offset;
                }
                else // JSRR
                {
                    uint16_t r1 = (instr >> 6) & 0x7;
                    reg[R_PC] = reg[r1];
                }

                break;
            }

            case OP_LD:
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

                reg[r0] = mem_read(reg[R_PC] + pc_offset);

                update_flags(r0);

                break;
            }

            case OP_LDI:
            {
                // Destination Register
                uint16_t r0 = (instr >> 9) & 0x7;

                // PCoffset9
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

                // Locate and retrieve data from the memory address
                // by adding PC_offset to current PC and load it in DR
                reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));

                update_flags(r0);

                break;
            }

            case OP_LDR:
            {
                uint16_t r0 = (instr >> 9) & 0x7;

                uint16_t r1 = (instr >> 6) & 0x7;

                uint16_t offset = sign_extend(instr & 0x3F, 6);

                reg[r0] = mem_read(reg[r1] + offset);

                update_flags(r0);

                break;
            }

            case OP_LEA:
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

                reg[r0] = reg[R_PC] + pc_offset;

                update_flags(r0);

                break;
            }

            case OP_ST:
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

                mem_write(reg[R_PC] + pc_offset, reg[r0]);

                break;
            }

            case OP_STI:
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

                mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);

                break;
            }

            case OP_STR:
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);

                mem_write(reg[r1] + offset, reg[r0]);

                break;
            }

            case OP_TRAP:
            {
                // Save return address
                reg[R_R7] = reg[R_PC];

                // Get trap code
                switch (instr & 0xFF)
                {
                    case TRAP_GETC:
                    {
                        // Read a single ASCII character 
                        reg[R_R0] = (uint16_t)getchar();
                        update_flags(R_R0);
                        break;
                    }

                    case TRAP_OUT:
                    {
                        putc((char)reg[R_R0], stdout);
                        fflush(stdout);
                        break;
                    }

                    case TRAP_PUTS:
                    {
                        /* one char per word */
                        uint16_t* c = memory + reg[R_R0]; // Performing pionter arithematic ; This line is also equivalent to &memory[0x3000] if R0 register is x3000
                        while (*c) // Continue looping until it reaches the null terminator (0x0000) 
                        {
                            putc((char)*c, stdout);
                            c++;
                        }
                        fflush(stdout);
                        break;
                    }

                    case TRAP_IN:
                    {
                        printf("Enter a character: ");
                        char c = getchar(); 
                        putc(c, stdout);
                        fflush(stdout);
                        reg[R_R0] = (uint16_t)c;
                        update_flags(R_R0);
                        break;
                    }

                    case TRAP_PUTSP:
                    {
                        /* one char per byte (two bytes per word)
                           here we need to swap back to
                           big endian format */
                        uint16_t* c = memory + reg[R_R0];
                        while (*c)
                        {
                            char char1 = (*c) & 0xFF;
                            putc(char1, stdout);
                            char char2 = (*c) >> 8;
                            if (char2) putc(char2, stdout);
                            ++c;
                        }
                        fflush(stdout);
                    }

                    case TRAP_HALT:
                    {
                        puts("HALT");
                        fflush(stdout);
                        running = 0; // Halts the program by effectively stopping the loop
                    }
                }

                break;
            }

            case OP_RES:
            case OP_RTI:
            default:
                break;
        }
    }

    restore_input_buffering();
    return 0;
}

// bit_count tells how many bits the original number uses.
// 0xFFFF in binary represents 1111 1111 1111 1111
uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1)
    {
        x |= (0xFFFF << bit_count);
    }

    return x;
}

// For updating condition flags.
// r is the index of register in which last operation took place.
// Last operation -> Result stored in register -> Pass the register index
// to this function -> access the value -> Check its nature -> update condition flags
void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZERO;
    }
    else if (reg[r] >> 15)
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}

void read_image_file(FILE* file)
{
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin); // LC-3 stores its 16 bits in big-endian. My machine stores it in little-endian. swap16 function fixes this mismatch.
    // we know the maximum file size so we only need one fread 
    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    // swap to little endian 
    while (read-- > 0)
    {
        *p = swap16(*p);
        ++p;
    }
}

uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

int read_image(const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if (!file) { return 0; };
    read_image_file(file);
    fclose(file);
    return 1;
}

void mem_write(uint16_t address, uint16_t val)
{
    memory[address] = val;
}

uint16_t mem_read(uint16_t address)
{
    if (address == MR_KBSR) {
        if (check_key()) {
            memory[MR_KBSR] = (1 << 15);  // Keyboard status status register (KBSR) 15th bit is the keyboard-ready flag. 
            memory[MR_KBSR] = getchar();
        }
        else 
        {
            memory[MR_KBSR] = 0;
        }
    }

    return memory[address];
}

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

