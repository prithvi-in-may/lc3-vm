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

The CPU doesn't understand English words. So we need an Assembler (which I will write later for LC3-VM and make example programs in it) which converts this assembly instruction into binary.

Also notice: Each line is of different length, which you might think of as a contradiction because we learnt that:

> "Every LC-3 instruction is 16-bits."

There is no contradiction. The text isn't the instruction the CPU sees. The assembler turns it into exactly 16 bits.

Also, `.ORIG x3000` is an assembler directive (like a macro). It tells the assembler:

> "Start putting this program into memory at address x3000."


## Learning Resource

This project is being built while following the LC-3 VM tutorial by Justin Meiners:

[LC-3 VM](https://www.jmeiners.com/lc3-vm/)
