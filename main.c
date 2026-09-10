#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#define MEMORY_MAX (1 << 16)

uint16_t memory[MEMORY_MAX];

uint16_t sign_extend(uint16_t x, int bit_count);
void update_flags(uint16_t r);

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

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }

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
                    uint16_t imm5 = sign_extend(instr & 0x1f, 5);
                    reg[r0] = reg[r1] + imm5;
                }
                else
                {
                    uint16_t r2 = instr & 0x7;
                    reg[r0] = reg[r1] + reg[r2];
                }

                update_flags(r0);
            }

            case OP_AND:
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t imm_flag = (instr >> 5) & 0x1;

                if (imm_flag)
                {
                    // immediate mode
                    uint16_t imm5 = sign_extend((instr >> 5) & 0x1F, int bit_count);
                    reg[r0] = reg[r1] & imm5;
                }
                else
                {
                    // register mode
                    uint16_t r2 = instr & 0x7;
                    reg[r0] = reg[r1] & reg[r2];
                }

                update_flags(r0);
            }

            case OP_NOT:
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;

                reg[r0] = ~reg[r1];

                update_flags(r0);
            }

            case OP_BR:
            {
                uint16_t pc_offset = (instr >> 9) & 0x1FF; // In binary, it represents 9 bits: 0000 0001 1111 1111

                // Condition codes
                uint16_t cond_flag = (instr >> 9) & 0x7;

                if (cond_flag & reg[R_COND]) // Checking with the condition flag. If it is a non-zero (011, 001, 100, etc)
                {
                    reg[R_PC] += pc_offset;
                }
            }

            case OP_JMP:
            {
                uint16_t r0 = (instr >> 6) & 0x7;

                reg[R_PC] = reg[r0];
            }

            case OP_JSR: // `JSR` Jumps to a subroutine using a PC-relative offset. `JSRR` jumps to a subroutine using an address stored in a register. NOTE: Both save the return address in `R7`
            {
                uint16_t flag = (instr >> 11) & 1; // (bit 11 = 1, otherwise 0 for JSRR).

                reg[R_R7] = reg[R_PC]; // Return address

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
            }

            case OP_LD: // Take a value from memory and load it in register
            {
                uint16_t r0 = (instr >> 9) & 0x7; // Desitination register
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

                reg[r0] = mem_read(reg[R_PC] + pc_offset);

                update_flags(r0);
            }

            case OP_LDI:
            {
                // Destination Register
                uint16_t r0 = (instr >> 9) & 0x7;

                // PCoffset9
                uint16_t PC_offset = sign_extend(instr * 0x1FF, 9);

                // Locate and retrieve data from the memory address by adding PC_offset to current PC and load it in DR
                reg[r0] = mem_read(mem_read(reg[R_PC] + PC_offset));

                update_flags(r0);
            }

            case OP_LDR: // Contents of memory at address is loaded in DR
            {
                uint16_t r0 = (instr >> 9) && 0x7;

                uint16_t r1 = (instr >> 6) && 0x7; // BaseR (Base Regiseter) contains the starting memory address with offset being the "distance" to move forward in momory and load the value into R2;

                uint16_t offset = sign_extend(instr & 0x3F, 6);

                reg[r0] = mem_read(reg[r1] + offset);

                update_flags(r0);
            }

            case OP_LEA: // Calculates an address using PC and puts the address into a register. Note: It does not memory.
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

                reg[r0] = reg[R_PC] + pc_offset;

                update_flags(r0);
            }

            case OP_ST: // Take the value from a register and store it into memory at an address calculated using the PC.
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

                mem_write(reg[R_PC] + pc_offset, reg[r0]);
            }

            case OP_STI:
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

                mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
            }

            case OP_STR:
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);

                mem_write(reg[r1] + offset, reg[r0]);
            }

            case OP_TRAP:
                break;

            case OP_RES:
            case OP_RTI:
            default:
                break;
        }
    }

    return 0;
}

// bit_count tells how many bits the original number uses. 0xFFFF in binary represents 1111 1111 1111 1111
uint16_t sign_exten(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1) & 1))
    {
        x |= (0xFFFF << bit_count);
    };

    return x;
}

// For updating condition flags. r is the index of register in which last operation took place.
// Last operation -> Result stored in register -> Pass the register index to this function -> access the value -> Check its nature -> update condition flags
void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] == FL_ZERO;
    }
    else if (reg[r] >> 15) // Checks whether the leftmost bit indicates negate as 1.
    {
        reg[R_COND] == FL_ZERO;
    }
    else
    {
        reg[R_COND] == FL_POS;
    }
}

