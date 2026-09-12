# LC-3 Virtual Machine

A from-scratch implementation of the Little Computer 3 (LC-3) virtual machine in C.

## Memory

We use a bitwise operator because it is necessary to express the structure or meaning of the number.

Compare:

```c
#define MEMORY_MAX 65536
```

and

```c
#define MEMORY_MAX (1 << 16)
```

`MEMORY_MAX` is `2¹⁶`.

If you're writing an LC-3 emulator, the architecture has 16-bit addresses. Therefore:

```text
number of possible addresses = 2^16
```

## Register

Hardware physically contains storage circuits which are capable of holding 16 bits, but the ISA or software abstraction actually gives it meaning by saying:

> This collection of 16 bits is register R0.

A CPU register being 16-bit means it has 16 positions for 0s and 1s. It can thus represent `2^16` different values.

There can be moments when a 16-bit register, or any register in specific operations, results in a value that may exceed its capacity to hold. In such cases, the software abstraction can simply combine two registers together. For example:

```text
One 16-bit register -> up to 2^16 = 65,536
Two 16-bit registers -> up to 2^32 = 4,294,967,296
```

In this LC3-VM, there are 10 registers. 8 are general-purpose registers. 2 have specific roles:

* **Condition flag:** It stores information about the result of the last operation. Such as, if the last operation resulted in 0, the CPU might set a zero flag. (`COND`)
* **Program counter:** Stores the address of the next instruction the CPU should execute. (`PC`)

## Instruction Set

`opcode` means operation code. It represents the CPU's instruction number.

LC-3 has 16 different fundamental operations that it understands. Each operation gets a 4-bit number such as `0001`, `0010`, etc.

Small instruction sets are referred to as RISCs, while larger ones are called CISCs.

## Condition Flag

`R_COND` stores the nature of the last operation result.

Take, for example:

```cpp
if (x > 0) {
    // Do something
}
```

At the machine level, the CPU has no way of knowing whether the last operation result was 0, negative, or positive. That's what the condition flags provide:

```text
P = positive
Z = zero
N = negative
```

In the code, the flags are provided with their own binary representation through the left bitwise shift operator.

The reason `1, 2, 3` weren't assigned here is:

With `1, 2, 3`:

```text
1 = 001   <- flag 1
2 = 010   <- flag 2
3 = 011   <- flag 3 ❌
```

The third flag uses both bit 0 and bit 1, which are already being used by the first two flags. So the flags overlap, and when you inspect `R_COND`, you can't cleanly tell which flag is responsible.

The problem is that a flag is supposed to correspond to one particular bit, and `3` corresponds to two bits at once.

With `1, 2, 4`:

```text
1 = 001   <- flag 1
2 = 010   <- flag 2
4 = 100   <- flag 3
```

Each flag gets its own independent bit. So `R_COND` can contain multiple flags without ambiguity.

Otherwise:

When you are using bit masking such as `NEG`, `POS`, `ZER` to check which one is enabled in `R_COND` through the `&` operation (for example, `R_COND & NEG` to check if the flag is negative), if we had `1, 2, 3`, then here is the problem:

For POS:

```text
001
 ↑
only look at bit 0
```

For ZERO:

```text
010
 ↑
only look at bit 1
```

For NEG:

```text
011
↑↑
```

It says: look at bit 0 **AND** bit 1.

So NEG doesn't have its own unique bit.

`R_COND & FL_NEG` ("Look inside `R_COND`. Is the NEG bit turned on?"):

```text
R_COND  = 010
FL_NEG  = 011
        -----
          010 <- WRONG
```

So C interprets:

```cpp
if (R_COND & FL_NEG)
```

as true, meaning "NEG is on!"

But NEG isn't on. It is zero, which is one because, in the code, it represents zero as `2`.

This is the reason why each flag should have its own unique and private bit.

So the important rule here for condition flags is:

> **One flag = One bit.**

This is called **bit masking**.

Thus, the correct implementation in the code is demonstrated as follows:

If ZERO is active:

```text
R_COND = 010

  010
& 100
-----
  000

Zero -> false -> NEG isn't active
```

If NEG really is active:

```text
R_COND = 100

  100
& 100
-----
  100

Non-zero -> true -> NEG is active.
```

## Assembly Example and Assembler

Imagine you are programming an LC-3 CPU:

```asm
.ORIG x3000
LEA R0, HELLO_STR
PUTs
HALT
HELLO_STR .STRINGZ "Hello World!"
.END
```

The CPU doesn't understand English words. So we need an Assembler, which converts this assembly instruction into binary.

Also notice: Each line is of different length, which you might think of as a contradiction because we learnt that:

> "Every LC-3 instruction is 16-bits."

There is no contradiction. The text isn't the instruction the CPU sees. The assembler turns it into exactly 16 bits.

Also, `.ORIG x3000` is an assembler directive (like a macro). It tells the assembler:

> "Start putting this program into memory at address x3000."


# Procedure

A CPU at its core is a loop. It repeatedly does 5 things:

1. Fetch instruction from memory whose address is given by the PC register.
2. Move PC to the next instruction.
3. Decode the opcode.
4. Execute the decoded instruction using the parameters supplied with the instruction.
5. Repeat.

This is a fetch-decode-execute cycle or an Instruction cycle.

Let me elaborate on each process now.

## 1. Fetch

```c
uint16_t instr = mem_read(reg[R_PC]++);
```

Suppose:

```text
R_PC = 0x3000
```

Remember: PC = Program Counter.

It contains the address of the instruction we are going to execute. So:

```text
mem_read(reg[R_PC])
```

means: Go to the memory address `0x3000` and give me whatever is stored there.

## 2. Using `mem_read(reg[R_PC]++)`

```c
mem_read(reg[R_PC]++);
```

means: read from memory `0x3000` and increment it by 1 so it becomes:

```text
0x3001
```

We can increment PC because normally the next instruction is stored in the next memory location. So that's how the CPU naturally moves through a program.

There is an important thing about Loops and jumps I want to clarify at CPU level.

Look at the assembly example again:

```asm
AND R0, R0, 0
LOOP
ADD R0, R0, 1
ADD R1, R0, -10
BRn LOOP
```

`LOOP` here represents a point in memory. It points to the `ADD R0, R0, 1` instruction in memory.

Now at the end of the second-last instruction, there is `BRn LOOP`.

`BRn` represents `Branch If Negative`.

`BR` is Branch -> change `PC` to another location.

`n` means only do it if the Negative condition flag is set.

This tells the CPU not to continue the instruction by pointing PC to the next instruction memory address, but instead go back to `LOOP`.

So the cycle can be:

```text
PC = 0x3005 -> fetch BRn -> condition is negative -> PC = 0x3002
```

This is how loops work at CPU level. All that happens is just:

```text
Change the PC register to somewhere else.
```

## 3. Getting the OPCODE

After fetching the instruction, we get a 16-bit instruction.

We need to tell the CPU what kind of instruction it is. For that, as we have decided on the format of instructions of LC-3, we know that the first 4 bits of the instruction are the opcode and the remaining 12 bits are the parameters of it.

```c
uint16_t op = instr >> 12;
```

We do `instr >> 12` because, for example:

```text
instr = 0001 000 000 1 00001
```

The first four bits are `0001`.

Shifting the whole thing 12 bits towards the right gives:

```text
0000 0000 0000 0001
```

So using the bitwise right shift operator, we extract the opcode.

## 4. Executing the instruction

The instructions are then executed within the while loop through the switch statement.

Also note: we have a variable `running` which is set to 1. This will make the while

# Implementing Instructions 

## ADD 
ADD has two modes: Immediate and Register mode. In Immediate mode, the two numbers to be added are embedded in the same instruction. It offers less space for the second number compared to Register mode.

In Immediate mode, only 5 bits are allowed for the second number, but since it is going to be added to a 16-bit number, this function extends it by adding 0s to match 16 bits. “Sign extension corrects this problem by filling in 0s for positive numbers and 1s for negative numbers, so that the original values are preserved.”

## LDI instruction

LDI means "load indirect". Data is retrieved from a given memory address through an offset.

We know that LC-3 instructions are of 16 bits. LDI instruction constitutes of 3 fields: OPCODE (12 to 15), Destination Register (9 to 11) and finally PCoffset9 (0 to 8). PCoffset9 field works by providing an offset for the PC to calculate the address where the data is supposed to be retrieved from. This is also a reason why we cannot directly put the address because the remaining field after OPCODE and DR is only of 9 bits whereas the address must be of 16 bits, so PC is used as a reference point for finding the address.

After PC is incremented and it points to the next instruction, PCoffset9 gives an offset for `PCoffset9` locations away starting from the PC to find the memory address containing the address I actually want.

The offset is calculated by the assembler and not the programmer.

## Branch instruction (Conditional Branch)

Branch instruction means "Look at the condition codes (N,Z,P). If the condition I asked for is true, Jump to a different location."

N -> Negative
P -> Positive
Z -> Zero

These are set through update_flags function in the code and represent the nature of the last instruction result.

In assembly, it can be represented as:

```asm
BRz LOOP
```

means branch if the result was Zero.

## Jump Instruction (`JMP`)

`JMP` doesn't store destination address in the instruction. It stores the number of a register whose contents contains the destination address.

Example:

```asm
JMP BaseR
```

means: Jump to the memory address stored inside BaseR.

> `RET` is a special case of `JMP`. When a function/subroutine is called with `JSR`, LC-3 stores the return address in `R7` register. So, `RET` simply jumps back to that address.

For example:

```asm
JSR FUNC ; Calling FUNC function
ADD R1, R1, #1 ; <- Return here after the FUNC function finishes

FUNC:
  ADD R2, R2, #5
  RET
```

So `JSR FUNC` is executed, LC-3:

1. Saves address of next instruction `ADD R1, R1, #1` into `R7`
2. Jumps to `FUNC`
3. `FUNC` eventually executes `RET`
4. `RET` does `PC = R7`

# Trap Routines

Three mechanisms are used for shifting focus when the CPU stops its current program and transfers control somewhere else, namely:

1. **Traps:** The program deliberately asks for OS help and is caused intentionally by the program. For example:

   ```text
   TRAP x20
   ```

   means: execute the OS routine for keyboard input.

2. **Exceptions:** It is caused by the CPU when it detects an unusual error/condition while executing an instruction. For example: division by 0, invalid instruction, etc.

3. **Interrupts:** It is generally caused by something outside the current instruction execution stream, often by hardware. For example: timer interrupt, keyboard interrupt, disk operation completion, etc.

In LC-3, each trap code is assigned a trap code (similar to OPCODE).

## PUTS

`PUTS` reads characters from LC-3 memory starting at the register stored in `R0`, and prints them until it finds `0x0000` (which is a null terminator in an LC-3 string).

# Loading Programs

The first 16 bytes of the file tells you where to put that specific program in which memory address. That is why the emulator later does:

```c
reg[R_PC] = 0x3000;
```

# Memory-Mapped Registers

Our LC-3 currently has 2 things:

1. CPU registers
2. Memory

The CPU registers are easy to access: `R0`, `R1`, `R2` ... `PC`, `COND`. The memory can be accessed as: `memory[0x0000]`, `memory[0x0001]`, ... `memory[0xFFFF]`.

We face a problem with our LC-3 communicating with the keyboard. How does the LC-3 program communicate with the keyboard?

We can solve this by pretending that the keyboard is a memory. So we reserve two memory addresses for the keyboard:

```text
0xFE00 → keyboard status
0xFE02 → keyboard data
```

These are called **memory-mapped registers**. Now the CPU can interact with the keyboard by accessing those addresses. So we need to reserve those addresses specially for the keyboard, otherwise when accessing them through `memory[0xFE00]` would result in just another ordinary array and wouldn't represent our keyboard.

So instead of letting the emulator directly access `memory[address]`, we have `mem_read(address)` and `mem_write(address, value)`. Now whenever the LC-3 wants to access or read memory, these functions will be used which will also intercept and check whether its a special hardware address or not.

> It may be valid to ask: why not use `getc()`? But then that would make the program regularly check for keyboard input, which will cost an unnecessary amount of resources. This is called `polling`.

# Compile and Run 

```
gcc main.c -o main
```

Run one of the `.obj` file: 
```
main ./rogue.obj
```

## Learning Resource

This project is being built while following the LC-3 VM tutorial by Justin Meiners:

[LC-3 VM](https://www.jmeiners.com/lc3-vm/)
[Project document](https://www.jmeiners.com/lc3-vm/supplies/lc3-isa.pdf)
