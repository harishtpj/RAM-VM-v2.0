#include <iostream>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

using namespace std;

#define MEMORY_MAX (1 << 16)


// Registers, Opcodes, Flags and Trap Codes
enum { R0 = 0, R1, R2, R3, R4, R5, R6, R7, RPC, RCND, RCNT };
enum { BR = 0, ADD, LD, ST, JSR, AND, LDR, STR, RTI, NOT, LDI, STI, JMP, RES, LEA, TRAP };
enum { FP = 1 << 0, FZ = 1 << 1, FN = 1 << 2, };
enum { TGETC = 0x20, TOUT = 0x21, TPUTS = 0x22, TIN = 0x23, TPUTSP = 0x24, THALT = 0x25 };
enum { MR_KBSR = 0xFE00, MR_KBDR = 0xFE02 }; // Memory Mapped Registers
enum { PC_START = 0x3000 };

namespace termio {
    struct termios original_tio;

    void disable_input_buffering() {
        tcgetattr(STDIN_FILENO, &original_tio);
        struct termios new_tio = original_tio;
        new_tio.c_lflag &= ~ICANON & ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    }

    void restore_input_buffering() {
        tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
    }

    uint16_t check_key() {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        return select(1, &readfds, NULL, NULL, &timeout) != 0;
    }

    void handle_interrupt(int signal) {
        restore_input_buffering();
        cout << endl;
        exit(-2);
    }
}

class RAM_VM {
    public:
        uint16_t memory[MEMORY_MAX];
        uint16_t reg[RCNT];

        RAM_VM() {
            reg[RCND] = FZ;
            reg[RPC] = PC_START;
        }

        template <unsigned op> 
        void ins(uint16_t instr);

        void fetch(uint16_t *instr, uint16_t *op) {
            *instr = mem_read(reg[RPC]++);
            *op = (*instr) >> 12;
        }

        void start() {
            bool running = true;
            while (running) {
                uint16_t instr, op;
                fetch(&instr, &op);
                
                switch (op) {
                    case ADD: {
                        /* destination register (DR) */
                        uint16_t r0 = (instr >> 9) & 0x7;
                        /* first operand (SR1) */
                        uint16_t r1 = (instr >> 6) & 0x7;
                        /* whether we are in immediate mode */
                        uint16_t imm_flag = (instr >> 5) & 0x1;

                        if (imm_flag) {
                            uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                            reg[r0] = reg[r1] + imm5;
                        } else {
                            uint16_t r2 = instr & 0x7;
                            reg[r0] = reg[r1] + reg[r2];
                        }

                        update_flags(r0);
                        break;
                    }

                    case AND: {
                        uint16_t r0 = (instr >> 9) & 0x7;
                        uint16_t r1 = (instr >> 6) & 0x7;
                        uint16_t imm_flag = (instr >> 5) & 0x1;

                        if (imm_flag) {
                            uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                            reg[r0] = reg[r1] & imm5;
                        } else {
                            uint16_t r2 = instr & 0x7;
                            reg[r0] = reg[r1] & reg[r2];
                        }

                        update_flags(r0);
                        break;
                    }

                    case NOT: {
                        uint16_t r0 = (instr >> 9) & 0x7;
                        uint16_t r1 = (instr >> 6) & 0x7;

                        reg[r0] = ~reg[r1];
                        update_flags(r0);
                        break;
                    }

                    case BR: {
                        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                        uint16_t cond_flag = (instr >> 9) & 0x7;

                        if (cond_flag & reg[RCND]) {
                            reg[RPC] += pc_offset;
                        }
                        break;
                    }

                    case JMP: {
                        /* Also handles RET */
                        uint16_t r1 = (instr >> 6) & 0x7;
                        reg[RPC] = reg[r1];
                        break;
                    }

                    case JSR: {
                        uint16_t long_flag = (instr >> 11) & 1;
                        reg[R7] = reg[RPC];

                        if (long_flag) {
                            uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
                            reg[RPC] += long_pc_offset;  /* JSR */
                        } else {
                            uint16_t r1 = (instr >> 6) & 0x7;
                            reg[RPC] = reg[r1]; /* JSRR */
                        }

                        break;
                    }

                    case LD: {
                        uint16_t r0 = (instr >> 9) & 0x7;
                        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                        reg[r0] = mem_read(reg[RPC] + pc_offset);
                        update_flags(r0);
                        break;
                    }

                    case LDI: {
                        /* destination register (DR) */
                        uint16_t r0 = (instr >> 9) & 0x7;
                        /* PCoffset 9*/
                        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                        /* add pc_offset to the current PC, look at that memory location to get the final address */
                        reg[r0] = mem_read(mem_read(reg[RPC] + pc_offset));
                        update_flags(r0);
                        break;
                    }

                    case LDR: {
                        uint16_t r0 = (instr >> 9) & 0x7;
                        uint16_t r1 = (instr >> 6) & 0x7;
                        uint16_t offset = sign_extend(instr & 0x3F, 6);
                        reg[r0] = mem_read(reg[r1] + offset);
                        update_flags(r0);
                        break;
                    }

                    case LEA: {
                        uint16_t r0 = (instr >> 9) & 0x7;
                        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                        reg[r0] = reg[RPC] + pc_offset;
                        update_flags(r0);
                        break;
                    }

                    case ST: {
                        uint16_t r0 = (instr >> 9) & 0x7;
                        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                        mem_write(reg[RPC] + pc_offset, reg[r0]);
                        break;
                    }

                    case STI: {
                        uint16_t r0 = (instr >> 9) & 0x7;
                        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                        mem_write(mem_read(reg[RPC] + pc_offset), reg[r0]);
                        break;
                    }

                    case STR: {
                        uint16_t r0 = (instr >> 9) & 0x7;
                        uint16_t r1 = (instr >> 6) & 0x7;
                        uint16_t offset = sign_extend(instr & 0x3F, 6);
                        mem_write(reg[r1] + offset, reg[r0]);
                        break;
                    }

                case TRAP: {
                    reg[R7] = reg[RPC];

                    switch (instr & 0xFF) {
                        case TGETC: {
                            /* read a single ASCII char */
                            reg[R0] = (uint16_t)getchar();
                            update_flags(R0);
                            break;
                        }

                        case TOUT: {
                            putc((char)reg[R0], stdout);
                            fflush(stdout);
                            break;
                        }

                        case TPUTS: {
                            /* one char per word */
                            uint16_t* c = memory + reg[R0];
                            while (*c) {
                                putc((char)*c, stdout);
                                ++c;
                            }
                            fflush(stdout);
                            break;
                        }

                        case TIN: {
                            cout << "Enter a character: ";
                            char c = getchar();
                            putc(c, stdout);
                            fflush(stdout);
                            reg[R0] = (uint16_t)c;
                            update_flags(R0);
                            break;
                        }

                        case TPUTSP: {
                            uint16_t* c = memory + reg[R0];
                            while (*c) {
                                char char1 = (*c) & 0xFF;
                                putc(char1, stdout);
                                char char2 = (*c) >> 8;
                                if (char2) putc(char2, stdout);
                                ++c;
                            }
                            fflush(stdout);
                            break;
                        }

                        case THALT: {
                            cout << endl << "RAM-VM Halted" << endl;
                            fflush(stdout);
                            running = false;
                            break;
                        }
                    }
                    break;
                }

                case RES:
                case RTI:
                default:
                    abort();
                    break;
                }
            }
            
        }

        void mem_write(uint16_t address, uint16_t val) {
            memory[address] = val;
        }

        uint16_t mem_read(uint16_t address) {
            if (address == MR_KBSR) {
                if (termio::check_key()) {
                    memory[MR_KBSR] = (1 << 15);
                    memory[MR_KBDR] = getchar();
                } else {
                    memory[MR_KBSR] = 0;
                }
            }
            return memory[address];
        }


        int read_image(const char* image_path) {
            FILE* file = fopen(image_path, "rb");
            if (!file) { return 0; };
            read_image_file(file);
            fclose(file);
            return 1;
        }
    
    private:
        uint16_t sign_extend(uint16_t x, int bit_count) {
            if ((x >> (bit_count - 1)) & 1)
                x |= (0xFFFF << bit_count);
            return x;
        }

        uint16_t swap16(uint16_t x) {
            return (x << 8) | (x >> 8);
        }

        void update_flags(uint16_t r) {
            if (reg[r] == 0) {
                reg[RCND] = FZ;
            } else if (reg[r] >> 15) {
                reg[RCND] = FN;
            } else {
                reg[RCND] = FP;
            }
        }

        void read_image_file(FILE* file) {
            /* the origin tells us where in memory to place the image */
            uint16_t origin;
            fread(&origin, sizeof(origin), 1, file);
            origin = swap16(origin);

            /* we know the maximum file size so we only need one fread */
            uint16_t max_read = MEMORY_MAX - origin;
            uint16_t* p = memory + origin;
            size_t read = fread(p, sizeof(uint16_t), max_read, file);

            /* swap to little endian */
            while (read-- > 0) {
                *p = swap16(*p);
                ++p;
            }
        }
};

int main(int argc, char** argv) {
    RAM_VM vm;

    if (argc < 2) {
        cout << "Usage: ram-vm [image-file1]..." << endl;
        exit(2);
    }

    for (int j = 1; j < argc; ++j) {
        if (!vm.read_image(argv[j])) {
            cout << "failed to load image: " << argv[j] << endl;
            exit(1);
        }
    }

    signal(SIGINT, termio::handle_interrupt);
    termio::disable_input_buffering();

    vm.start();

    termio::restore_input_buffering();
}