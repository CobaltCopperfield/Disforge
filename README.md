# Disforge

A lightweight x86 instruction disassembler written in C that converts machine code bytes into human-readable assembly instructions. The disforge supports common 32-bit x86 instructions and addressing modes.

## Features

- Decodes common x86 instructions including:
  - MOV (register, memory, and immediate variants)
  - Arithmetic operations (ADD, SUB, MUL, DIV, etc.)
  - Control flow (JMP, CALL, RET, conditional jumps)
  - Stack operations (PUSH, POP)
  - Bit manipulation (AND, OR, XOR, shifts)
  - String operations (MOVSB, STOSB, etc.)
  - Sign/zero extension (MOVZX, MOVSX)
- Supports multiple addressing modes:
  - Register-direct addressing
  - Memory addressing with displacement
  - SIB (Scale-Index-Base) addressing
- Provides offset information for each instruction
- Handles instruction prefixes (LOCK, REP, REPNZ)

## Building

To compile the disforge, use a C compiler such as GCC:

```bash
gcc -o disforge disforge.c
```

## Usage

The disforge can be used in two ways:

1. Built-in test mode (default):
   
   ```bash
   ./disforge
   ```
   This will disassemble a built-in set of test instructions.

2. File input mode (requires uncommenting the alternative main function):

   ```bash
   ./disforge <machine_code_file>
   ```
   This will disassemble machine code from the specified binary file.

## Output Format

The disforge outputs each instruction in the following format:

```bash
offset: instruction operands
```

Example output:

```bash
0000: NOP
0001: MOV EAX, 0x12345678
0006: MOV ECX, 0x90ABCDEF
000b: ADD EAX, ECX
```

## Limitations

- Only supports 32-bit x86 instructions
- Limited instruction set coverage
- No support for floating-point instructions
- No support for MMX/SSE instructions
- Only handles basic prefixes

## Implementation Details

The disforge works by:

1. Reading machine code bytes sequentially
2. Identifying instruction opcodes
3. Decoding ModR/M and SIB bytes when present
4. Handling immediate values and displacements
5. Formatting the output in AT&T syntax

Key functions:

- ```disassemble()```: Main disassembly routine
- ```decode_rm_operand()```: Decodes ModR/M addressing modes
- ```print_reg()```: Prints register names
- ```print_condition()```: Prints condition codes for conditional jumps

## Contributing

Feel free to contribute by:

- Adding support for more instructions
- Improving error handling
- Adding support for different syntax styles
- Enhancing documentation
- Fixing bugs

## Notes

This is a basic disassembler intended for educational purposes. For production use, consider using established disassembly frameworks like Capstone or LLVM.
