#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>

// A static list of general–purpose register names.
static const char *reg_names[] = {"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"};

// Print a register name (using only the low 3 bits)
void print_reg(uint8_t reg) {
    printf("%s", reg_names[reg & 0x7]);
}

// Print condition codes (using low 4 bits)
void print_condition(uint8_t code) {
    const char* conditions[] = {"O", "NO", "B/NAE/C", "NB/AE/NC", "E/Z", "NE/NZ", "BE/NA", "NBE/A",
                                "S", "NS", "P/PE", "NP/PO", "L/NGE", "NL/GE", "LE/NG", "NLE/G"};
    printf("%s", conditions[code & 0xF]);
}

/*
 * decode_rm_operand() decodes an operand given a ModR/M byte.
 *   modrm  - the ModR/M byte
 *   code   - the pointer to the machine code
 *   index  - the current index (just after the ModR/M byte)
 *   code_size - total available bytes
 *   buffer - where to write the operand string
 *   bufsize - size of buffer
 *
 * Returns the new index after consuming any SIB/displacement bytes.
 *
 * For mod==3 the operand is simply a register;
 * otherwise the operand is printed as a memory reference (with a simple SIB/displacement decoder).
 */
size_t decode_rm_operand(uint8_t modrm, uint8_t *code, size_t index, size_t code_size,
                           char *buffer, size_t bufsize)
{
    int len = 0;
    uint8_t mod = modrm >> 6;
    uint8_t rm  = modrm & 0x7;

    if (mod == 3) {
        // Register operand
        len += snprintf(buffer + len, bufsize - len, "%s", reg_names[rm]);
        return index;
    }

    // Memory operand – start with an opening bracket
    len += snprintf(buffer + len, bufsize - len, "[");
    if (rm == 4) {
        // SIB byte present
        if (index >= code_size) {
            len += snprintf(buffer + len, bufsize - len, "incomplete SIB");
            return index;
        }
        uint8_t sib = code[index++];
        uint8_t scale = sib >> 6;
        uint8_t index_reg = (sib >> 3) & 0x7;
        uint8_t base = sib & 0x7;

        // If mod == 0 and base == 5, then no base register (disp32 only)
        if (!(mod == 0 && base == 5)) {
            len += snprintf(buffer + len, bufsize - len, "%s", reg_names[base]);
        }
        // If the index field is not 4 (which means “none”), add index register
        if (index_reg != 4) {
            if (len > 0)
                len += snprintf(buffer + len, bufsize - len, " + ");
            len += snprintf(buffer + len, bufsize - len, "%s", reg_names[index_reg]);
            if (scale > 0)
                len += snprintf(buffer + len, bufsize - len, "*%d", 1 << scale);
        }
    } else {
        // No SIB – simply print the register from the rm field.
        len += snprintf(buffer + len, bufsize - len, "%s", reg_names[rm]);
    }

    // Now check for any displacement bytes.
    if (mod == 1) {
        // disp8
        if (index >= code_size) {
            len += snprintf(buffer + len, bufsize - len, " + <incomplete disp8>");
            return index;
        }
        int8_t disp = (int8_t) code[index++];
        if (disp < 0)
            len += snprintf(buffer + len, bufsize - len, " - 0x%x", -disp);
        else
            len += snprintf(buffer + len, bufsize - len, " + 0x%x", disp);
    } else if (mod == 2 || (mod == 0 && rm == 5)) {
        // disp32 (or mod==0, rm==5 means disp32 with no base)
        if (index + 4 > code_size) {
            len += snprintf(buffer + len, bufsize - len, " + <incomplete disp32>");
            return index;
        }
        int32_t disp = *(int32_t*)&code[index];
        index += 4;
        if (disp < 0)
            len += snprintf(buffer + len, bufsize - len, " - 0x%x", -disp);
        else
            len += snprintf(buffer + len, bufsize - len, " + 0x%x", disp);
    }
    len += snprintf(buffer + len, bufsize - len, "]");
    return index;
}

/*
 * disassemble() reads the given machine code (of code_size bytes) and prints
 * a textual disassembly. For instructions that use a ModR/M byte the function
 * calls decode_rm_operand() to print memory–addressing.
 */
void disassemble(uint8_t *code, size_t code_size) {
    size_t i = 0;
    while (i < code_size) {
        printf("%04zx: ", i);
        uint8_t opcode = code[i];

        // For instructions that need to be followed by a ModR/M byte,
        // we check that enough bytes remain.
        switch (opcode) {
            // MOV instructions with ModR/M (0x88-0x8B)
            case 0x88: case 0x89: case 0x8A: case 0x8B: {
                if (i + 1 >= code_size) {
                    printf("Incomplete MOV instruction");
                    return;
                }
                uint8_t modrm = code[i + 1];
                size_t next = i + 2;
                char operand_buf[64];
                printf("MOV ");
                if (opcode & 0x02) {
                    // Direction = 1: destination = reg field; source = r/m operand.
                    print_reg((modrm >> 3) & 0x7);
                    printf(", ");
                    next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                    printf("%s", operand_buf);
                } else {
                    // Direction = 0: destination = r/m operand; source = reg field.
                    next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                    printf("%s, ", operand_buf);
                    print_reg((modrm >> 3) & 0x7);
                }
                i = next;
                break;
            }

            // MOV immediate (8-bit) to register: 0xB0 ... 0xB7
            case 0xB0 ... 0xB7:
                if (i + 1 >= code_size) {
                    printf("Incomplete MOV imm8");
                    return;
                }
                printf("MOV %s, 0x%02x", reg_names[opcode & 0x7], code[i + 1]);
                i += 2;
                break;

            // MOV immediate (32-bit) to register: 0xB8 ... 0xBF
            case 0xB8 ... 0xBF:
                if (i + 4 >= code_size) {
                    printf("Incomplete MOV imm32");
                    return;
                }
                printf("MOV %s, 0x%08x", reg_names[opcode & 0x7],
                       *(uint32_t*)&code[i + 1]);
                i += 5;
                break;

            // Arithmetic instructions (using ModR/M)
            case 0x00 ... 0x05: case 0x08 ... 0x0D: case 0x10 ... 0x15:
            case 0x18 ... 0x1D: case 0x20 ... 0x25: case 0x28 ... 0x2D:
            case 0x30 ... 0x35: case 0x38 ... 0x3D: {
                if (i + 1 >= code_size) {
                    printf("Incomplete arithmetic instruction");
                    return;
                }
                uint8_t modrm = code[i + 1];
                size_t next = i + 2;
                char operand_buf[64];
                const char* ops[] = {"ADD", "OR", "ADC", "SBB", "AND", "SUB", "XOR", "CMP"};
                printf("%s ", ops[(opcode >> 3) & 0x7]);
                if (opcode & 0x02) {
                    // destination = reg field; source = r/m operand.
                    print_reg((modrm >> 3) & 0x7);
                    printf(", ");
                    next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                    printf("%s", operand_buf);
                } else {
                    // destination = r/m operand; source = reg field.
                    next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                    printf("%s, ", operand_buf);
                    print_reg((modrm >> 3) & 0x7);
                }
                i = next;
                break;
            }

            // Immediate arithmetic (0x80, 0x81, 0x83)
            case 0x80: case 0x81: case 0x83: {
                if (i + 1 >= code_size) {
                    printf("Incomplete immediate arithmetic");
                    return;
                }
                uint8_t modrm = code[i + 1];
                size_t next = i + 2;
                char operand_buf[64];
                const char* ops[] = {"ADD", "OR", "ADC", "SBB", "AND", "SUB", "XOR", "CMP"};
                uint8_t op = (modrm >> 3) & 0x7;
                printf("%s ", ops[op]);
                next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                printf("%s, ", operand_buf);
                if (opcode == 0x81) {
                    if (next + 4 > code_size) {
                        printf("Incomplete immediate");
                        return;
                    }
                    uint32_t imm = *(uint32_t*)&code[next];
                    next += 4;
                    printf("0x%08x", imm);
                } else {
                    if (next >= code_size) {
                        printf("Incomplete immediate");
                        return;
                    }
                    uint8_t imm = code[next];
                    next += 1;
                    printf("0x%02x", imm);
                }
                i = next;
                break;
            }

            // INC/DEC (register–only instructions)
            case 0x40 ... 0x47:
                printf("INC %s", reg_names[opcode & 0x7]);
                i++;
                break;
            case 0x48 ... 0x4F:
                printf("DEC %s", reg_names[opcode & 0x7]);
                i++;
                break;

            // PUSH/POP (register instructions)
            case 0x50 ... 0x57:
                printf("PUSH %s", reg_names[opcode & 0x7]);
                i++;
                break;
            case 0x58 ... 0x5F:
                printf("POP %s", reg_names[opcode & 0x7]);
                i++;
                break;

            // PUSH immediate 32-bit and 8-bit
            case 0x68:
                if (i + 4 >= code_size) {
                    printf("Incomplete PUSH imm32");
                    return;
                }
                printf("PUSH 0x%08x", *(uint32_t*)&code[i + 1]);
                i += 5;
                break;
            case 0x6A:
                if (i + 1 >= code_size) {
                    printf("Incomplete PUSH imm8");
                    return;
                }
                printf("PUSH 0x%02x", code[i + 1]);
                i += 2;
                break;

            // MOV r/m8, imm8 and MOV r/m32, imm32
            case 0xC6: {
                if (i + 1 >= code_size) {
                    printf("Incomplete MOV r/m8, imm8");
                    return;
                }
                uint8_t modrm = code[i + 1];
                size_t next = i + 2;
                char operand_buf[64];
                printf("MOV ");
                next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                printf("%s, ", operand_buf);
                if (next >= code_size) {
                    printf("Incomplete immediate");
                    return;
                }
                printf("0x%02x", code[next]);
                next++;
                i = next;
                break;
            }
            case 0xC7: {
                if (i + 1 >= code_size) {
                    printf("Incomplete MOV r/m32, imm32");
                    return;
                }
                uint8_t modrm = code[i + 1];
                size_t next = i + 2;
                char operand_buf[64];
                printf("MOV ");
                next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                printf("%s, ", operand_buf);
                if (next + 4 > code_size) {
                    printf("Incomplete immediate");
                    return;
                }
                uint32_t imm = *(uint32_t*)&code[next];
                next += 4;
                printf("0x%08x", imm);
                i = next;
                break;
            }

            // Conditional jump instructions (0x70 ... 0x7F)
            case 0x70 ... 0x7F:
                if (i + 1 >= code_size) {
                    printf("Incomplete conditional jump");
                    return;
                }
                printf("J");
                print_condition(opcode);
                printf(" 0x%02x", code[i + 1]);
                i += 2;
                break;

            // CALL rel32, JMP rel32, JMP rel8
            case 0xE8:
                if (i + 4 >= code_size) {
                    printf("Incomplete CALL");
                    return;
                }
                printf("CALL 0x%08" PRIxPTR, (size_t)(*(int32_t*)&code[i + 1] + i + 5));
                i += 5;
                break;
            case 0xE9:
                if (i + 4 >= code_size) {
                    printf("Incomplete JMP");
                    return;
                }
                printf("JMP 0x%08" PRIxPTR, (size_t)(*(int32_t*)&code[i + 1] + i + 5));
                i += 5;
                break;
            case 0xEB:
                if (i + 1 >= code_size) {
                    printf("Incomplete JMP rel8");
                    return;
                }
                printf("JMP 0x%02x", code[i + 1]);
                i += 2;
                break;

            // Other instructions
            case 0x90:
                printf("NOP");
                i++;
                break;
            case 0xC3:
                printf("RET");
                i++;
                break;
            case 0xCC:
                printf("INT3");
                i++;
                break;

            // LEA
            case 0x8D: {
                if (i + 1 >= code_size) {
                    printf("Incomplete LEA");
                    return;
                }
                uint8_t modrm = code[i + 1];
                size_t next = i + 2;
                char operand_buf[64];
                printf("LEA ");
                print_reg((modrm >> 3) & 0x7);
                printf(", ");
                next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                printf("%s", operand_buf);
                i = next;
                break;
            }

            // TEST
            case 0x84: case 0x85: {
                if (i + 1 >= code_size) {
                    printf("Incomplete TEST");
                    return;
                }
                uint8_t modrm = code[i + 1];
                size_t next = i + 2;
                char operand_buf[64];
                printf("TEST ");
                next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                printf("%s, ", operand_buf);
                print_reg((modrm >> 3) & 0x7);
                i = next;
                break;
            }

            // XCHG
            case 0x86: case 0x87: {
                if (i + 1 >= code_size) {
                    printf("Incomplete XCHG");
                    return;
                }
                uint8_t modrm = code[i + 1];
                size_t next = i + 2;
                char operand_buf[64];
                printf("XCHG ");
                next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                printf("%s, ", operand_buf);
                print_reg((modrm >> 3) & 0x7);
                i = next;
                break;
            }

            // Shift/Rotate instructions
            case 0xC0: case 0xC1: case 0xD0: case 0xD1: case 0xD2: case 0xD3: {
                if (i + 1 >= code_size) {
                    printf("Incomplete shift/rotate");
                    return;
                }
                uint8_t modrm = code[i + 1];
                size_t next = i + 2;
                char operand_buf[64];
                const char* ops[] = {"ROL", "ROR", "RCL", "RCR", "SHL", "SHR", "SAL", "SAR"};
                printf("%s ", ops[(modrm >> 3) & 0x7]);
                next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                printf("%s, ", operand_buf);
                if (opcode == 0xD2 || opcode == 0xD3) {
                    printf("CL");
                } else if (opcode == 0xC0 || opcode == 0xC1) {
                    if (next >= code_size) {
                        printf("Incomplete immediate");
                        return;
                    }
                    printf("0x%02x", code[next]);
                    next++;
                } else {
                    printf("1");
                }
                i = next;
                break;
            }

            // MUL/IMUL/DIV/IDIV
            case 0xF6: case 0xF7: {
                if (i + 1 >= code_size) {
                    printf("Incomplete MUL/IMUL/DIV/IDIV");
                    return;
                }
                uint8_t modrm = code[i + 1];
                size_t next = i + 2;
                char operand_buf[64];
                uint8_t reg_op = (modrm >> 3) & 0x7;
                const char* ops[] = {"TEST", "TEST", "NOT", "NEG", "MUL", "IMUL", "DIV", "IDIV"};
                printf("%s ", ops[reg_op]);
                next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                printf("%s", operand_buf);
                if (reg_op == 0 || reg_op == 1) {
                    // TEST instructions use an immediate operand
                    if (opcode == 0xF6) {
                        if (next >= code_size) { printf(" <incomplete imm>"); return; }
                        printf(", 0x%02x", code[next]);
                        next++;
                    } else {
                        if (next + 4 > code_size) { printf(" <incomplete imm>"); return; }
                        uint32_t imm = *(uint32_t*)&code[next];
                        next += 4;
                        printf(", 0x%08x", imm);
                    }
                }
                i = next;
                break;
            }

            // MOVZX/MOVSX (two–byte opcodes, 0x0F xx)
            case 0x0F: {
                if (i + 2 >= code_size) {
                    printf("Incomplete 0F instruction");
                    i++;
                    break;
                }
                uint8_t second_byte = code[i + 1];
                uint8_t modrm = code[i + 2];
                size_t next = i + 3;
                char operand_buf[64];
                if (second_byte == 0xB6 || second_byte == 0xB7) {
                    printf("MOVZX ");
                    print_reg((modrm >> 3) & 0x7);
                    printf(", ");
                    if (second_byte == 0xB6)
                        printf("BYTE PTR ");
                    next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                    printf("%s", operand_buf);
                } else if (second_byte == 0xBE || second_byte == 0xBF) {
                    printf("MOVSX ");
                    print_reg((modrm >> 3) & 0x7);
                    printf(", ");
                    if (second_byte == 0xBE)
                        printf("BYTE PTR ");
                    next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                    printf("%s", operand_buf);
                } else {
                    printf("Unknown 0F instruction");
                    next = i + 2;
                }
                i = next;
                break;
            }

            // CALL/JMP indirect (0xFF)
            case 0xFF: {
                if (i + 1 >= code_size) {
                    printf("Incomplete FF instruction");
                    return;
                }
                uint8_t modrm = code[i + 1];
                size_t next = i + 2;
                char operand_buf[64];
                uint8_t reg_op = (modrm >> 3) & 0x7;
                if (reg_op == 0) {
                    printf("INC ");
                    next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                    printf("%s", operand_buf);
                } else if (reg_op == 1) {
                    printf("DEC ");
                    next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                    printf("%s", operand_buf);
                } else if (reg_op == 2) {
                    printf("CALL ");
                    next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                    printf("%s", operand_buf);
                } else if (reg_op == 4) {
                    printf("JMP ");
                    next = decode_rm_operand(modrm, code, next, code_size, operand_buf, sizeof(operand_buf));
                    printf("%s", operand_buf);
                } else {
                    printf("Unknown FF instruction");
                }
                i = next;
                break;
            }

            // LOOP instructions
            case 0xE0:
                if (i + 1 >= code_size) {
                    printf("Incomplete LOOPNZ");
                    return;
                }
                printf("LOOPNZ 0x%02x", code[i + 1]);
                i += 2;
                break;
            case 0xE1:
                if (i + 1 >= code_size) {
                    printf("Incomplete LOOPZ");
                    return;
                }
                printf("LOOPZ 0x%02x", code[i + 1]);
                i += 2;
                break;
            case 0xE2:
                if (i + 1 >= code_size) {
                    printf("Incomplete LOOP");
                    return;
                }
                printf("LOOP 0x%02x", (uint8_t)(i + 2 + (int8_t)code[i + 1]));
                i += 2;
                break;
            case 0xE3:
                if (i + 1 >= code_size) {
                    printf("Incomplete JECXZ");
                    return;
                }
                printf("JECXZ 0x%02x", code[i + 1]);
                i += 2;
                break;

            // String operations
            case 0xA4:
                printf("MOVSB");
                i++;
                break;
            case 0xA5:
                printf("MOVSD");
                i++;
                break;
            case 0xA6:
                printf("CMPSB");
                i++;
                break;
            case 0xA7:
                printf("CMPSD");
                i++;
                break;
            case 0xAA:
                printf("STOSB");
                i++;
                break;
            case 0xAB:
                printf("STOSD");
                i++;
                break;
            case 0xAC:
                printf("LODSB");
                i++;
                break;
            case 0xAD:
                printf("LODSD");
                i++;
                break;
            case 0xAE:
                printf("SCASB");
                i++;
                break;
            case 0xAF:
                printf("SCASD");
                i++;
                break;

            // Prefix bytes
            case 0xF0:
                printf("LOCK ");
                i++;
                break;
            case 0xF2:
                printf("REPNZ ");
                i++;
                break;
            case 0xF3: {
                printf("REP ");
                i++;
                if (i < code_size) {
                    opcode = code[i];
                    if (opcode == 0xA4) {
                        printf("MOVSB");
                        i++;
                    } else if (opcode == 0xA5) {
                        printf("MOVSD");
                        i++;
                    } else {
                        printf("Unknown REP instruction");
                        i++;
                    }
                } else {
                    printf("Incomplete REP instruction");
                    i++;
                }
                break;
            }

            default:
                printf("Unknown instruction: 0x%02x", opcode);
                i++;
                break;
        }
        printf("\n");
    }
}

/* Uncomment this section if you want to test with a binary file
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <machine_code_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    // Determine file size
    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("Error seeking file");
        fclose(fp);
        return EXIT_FAILURE;
    }
    long file_size = ftell(fp);
    if (file_size < 0) {
        perror("Error determining file size");
        fclose(fp);
        return EXIT_FAILURE;
    }
    rewind(fp);

    // Allocate buffer to hold the machine code.
    uint8_t *buffer = malloc(file_size);
    if (!buffer) {
        perror("Memory allocation error");
        fclose(fp);
        return EXIT_FAILURE;
    }

    size_t read = fread(buffer, 1, file_size, fp);
    if (read != (size_t)file_size) {
        fprintf(stderr, "Error reading file (read %zu bytes, expected %ld)\n", read, file_size);
        free(buffer);
        fclose(fp);
        return EXIT_FAILURE;
    }
    fclose(fp);

    printf("Disassembled code from file '%s':\n", filename);
    disassemble(buffer, read);

    free(buffer);
    return EXIT_SUCCESS;
}
*/

int main() {
    // Example machine code containing a variety of instructions.
    uint8_t code[] = {
        0x90,                               // NOP
        0xB8, 0x78, 0x56, 0x34, 0x12,         // MOV EAX, 0x12345678
        0xB9, 0xEF, 0xCD, 0xAB, 0x90,         // MOV ECX, 0x90ABCDEF
        0x03, 0xC1,                         // ADD EAX, ECX   (ModR/M: both operands registers)
        0x83, 0xE8, 0x05,                   // SUB EAX, 5     (immediate arithmetic)
        0x89, 0xC3,                         // MOV EBX, EAX
        0x01, 0xCB,                         // ADD EBX, ECX
        0x29, 0xC3,                         // SUB EBX, EAX
        0xF7, 0xE3,                         // MUL EBX       (F7 /4)
        0xE8, 0x12, 0x34, 0x56, 0x78,         // CALL 0x78563412
        0x74, 0x05,                         // JE +5
        0xE9, 0x78, 0x56, 0x34, 0x12,         // JMP 0x12345678
        0xFF, 0xC0,                         // INC EAX       (FF /0)
        0xFF, 0xC8,                         // DEC EAX       (FF /1)
        0x0F, 0xB6, 0xC0,                   // MOVZX EAX, AL
        0x0F, 0xBE, 0xC0,                   // MOVSX EAX, AL
        0xF3, 0xA4,                         // REP MOVSB
        0x86, 0xC1,                         // XCHG AL, CL
        0xD1, 0xE0,                         // SHL EAX, 1
        0xE2, 0xFE,                         // LOOP -2
        0xC3                                // RET
    };
    size_t code_size = sizeof(code) / sizeof(code[0]);
    
    printf("Disassembled code:\n");
    disassemble(code, code_size);
    
    return 0;
}