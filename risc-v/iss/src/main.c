#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "main.h"

/***************************************************************/
/* Main memory.                                                */
/***************************************************************/


typedef struct {
    uint32_t start, size;
    uint8_t *mem;
} mem_region_t;

/* memory will be dynamically allocated at initialization */
mem_region_t MEM_REGIONS[] = {
    { MEM_TEXT_START, MEM_TEXT_SIZE, NULL },
    { MEM_DATA_START, MEM_DATA_SIZE, NULL },
    { MEM_STACK_START, MEM_STACK_SIZE, NULL },
    { MEM_KDATA_START, MEM_KDATA_SIZE, NULL },
    { MEM_KTEXT_START, MEM_KTEXT_SIZE, NULL }
};

#define MEM_NREGIONS (sizeof(MEM_REGIONS)/sizeof(mem_region_t))

/***************************************************************/
/* CPU State info.                                             */
/***************************************************************/

CPU_State CURRENT_STATE, NEXT_STATE;
int RUN_BIT;	/* run bit */
int INSTRUCTION_COUNT;
int prev_pc;
int instr_count;
int my_count = 0;

/***************************************************************/
/*                                                             */
/* Procedure: mem_read_32                                      */
/*                                                             */
/* Purpose: Read a 32-bit word from memory                     */
/*                                                             */
/***************************************************************/
uint32_t mem_read_32(uint32_t address)
{
    uint32_t i;
    for (i = 0; i < MEM_NREGIONS; i++) {
        if (address >= MEM_REGIONS[i].start &&
                address < (MEM_REGIONS[i].start + MEM_REGIONS[i].size)) {
            uint32_t offset = address - MEM_REGIONS[i].start;

            return
                (MEM_REGIONS[i].mem[offset+3] << 24) |
                (MEM_REGIONS[i].mem[offset+2] << 16) |
                (MEM_REGIONS[i].mem[offset+1] <<  8) |
                (MEM_REGIONS[i].mem[offset+0] <<  0);
        }
    }

    return 0;
}

/***************************************************************/
/*                                                             */
/* Procedure: mem_write_32                                     */
/*                                                             */
/* Purpose: Write a 32-bit word to memory                      */
/*                                                             */
/***************************************************************/
void mem_write_32(uint32_t address, uint32_t value)
{
    uint32_t i;
    for (i = 0; i < MEM_NREGIONS; i++) {
        if (address >= MEM_REGIONS[i].start &&
                address < (MEM_REGIONS[i].start + MEM_REGIONS[i].size)) {
            uint32_t offset = address - MEM_REGIONS[i].start;

            MEM_REGIONS[i].mem[offset+3] = (value >> 24) & 0xFF;
            MEM_REGIONS[i].mem[offset+2] = (value >> 16) & 0xFF;
            MEM_REGIONS[i].mem[offset+1] = (value >>  8) & 0xFF;
            MEM_REGIONS[i].mem[offset+0] = (value >>  0) & 0xFF;
            return;
        }
    }
}

/***************************************************************/
/*                                                             */
/* Procedure : cycle                                           */
/*                                                             */
/* Purpose   : Execute a cycle                                 */
/*                                                             */
/***************************************************************/
void cycle() {                                                

  process_instruction();
  CURRENT_STATE = NEXT_STATE;
  INSTRUCTION_COUNT++;
}

/***************************************************************/
/*                                                             */
/* Procedure : run n                                           */
/*                                                             */
/* Purpose   : Simulate RISC-V for n cycles                      */
/*                                                             */
/***************************************************************/
extern void run(int num_cycles) {                                      
  int i;

  for (i = 0; i < num_cycles; i++) {
    if (RUN_BIT == FALSE) {
	    printf("Simulator halted\n");
	    exit (0);
    }
    cycle();
  }
}

/***************************************************************/
/*                                                             */
/* Procedure : go                                              */
/*                                                             */
/* Purpose   : Simulate RISC-V until HALTed                      */
/*                                                             */
/***************************************************************/
void go() {                                                     
  if (RUN_BIT == FALSE) {
    printf("Can't simulate, Simulator is halted\n");
    return;
  }

  printf("Simulating...\n");
  while (RUN_BIT)
    cycle();
  printf("Simulator halted\n");
}

/***************************************************************/ 
/*                                                             */
/* Procedure : mdump                                           */
/*                                                             */
/* Purpose   : Dump a word-aligned region of memory to the     */
/*             output file.                                    */
/*                                                             */
/***************************************************************/
void mdump(/*FILE * dumpsim_file,*/ int start, int stop) {          
  int address;

  printf("\nMemory content [0x%08x..0x%08x] :\n", start, stop);
  printf("-------------------------------------\n");
  for (address = start; address <= stop; address += 4)
    printf("  0x%08x (%d) : 0x%08x\n", address, address, mem_read_32(address));
  printf("\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : rdump                                           */
/*                                                             */
/* Purpose   : Dump current register and bus values to the     */   
/*             output file.                                    */
/*                                                             */
/***************************************************************/
void rdump(FILE * dumpsim_file) {                               
  int k; 

  printf("\nCurrent register/bus values :\n");
  printf("-------------------------------------\n");
  printf("Instruction Count : %u\n", INSTRUCTION_COUNT);
  printf("PC                : 0x%08x\n", CURRENT_STATE.PC);
  printf("Registers:\n");
  for (k = 0; k < RISCV_REGS; k++)
    printf("R%d: 0x%08x\n", k, CURRENT_STATE.REGS[k]);
  printf("\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : init_memory                                     */
/*                                                             */
/* Purpose   : Allocate and zero memory                        */
/*                                                             */
/***************************************************************/
void init_memory() {                                           
    uint32_t i;
    for (i = 0; i < MEM_NREGIONS; i++) {
        MEM_REGIONS[i].mem = malloc(MEM_REGIONS[i].size);
        memset(MEM_REGIONS[i].mem, 0xdeadbeef, MEM_REGIONS[i].size);
    }
    #ifdef PRELOAD_ARCH_REGS
    printf ("Preloading architecture registers\n");
    for (i = 0; i < RISCV_REGS; i++) {
        CURRENT_STATE.REGS[i] = (i*32 + i%3);
    }
    #endif
    CURRENT_STATE.PC = MEM_TEXT_START;
    prev_pc = MEM_TEXT_START;
    NEXT_STATE = CURRENT_STATE;
      
    RUN_BIT = TRUE;
}

/**************************************************************/
/*                                                            */
/* Procedure : load_instr_opcode                              */
/*                                                            */
/* Purpose   : Load instruction opcode into mem               */
/*                                                            */
/**************************************************************/
void load_instr_opcode (uint32_t instr_opcode) {
    mem_write_32(CURRENT_STATE.PC, instr_opcode);
}

/**************************************************************/
/*                                                            */
/* Procedure : load_program                                   */
/*                                                            */
/* Purpose   : Load program and service routines into mem.    */
/*                                                            */
/**************************************************************/
void load_program(char *program_filename, char *pc_filename) {                   
  FILE * prog; 
  FILE * pc_file;
  int ii, word, pc_word;

  /* Open program file. */
  prog = fopen(program_filename, "r");
  /* Open PC file. */
  pc_file = fopen (pc_filename, "r");

  if (prog == NULL) {
    printf("Error: Can't open program file %s\n", program_filename);
    exit(-1);
  }

  /* Read in the program. */

  ii = 0;
  while (fscanf(prog, "%x\n", &word) != EOF) {
    if (fscanf (pc_file, "%x\n", &pc_word) != EOF) {
        mem_write_32(pc_word, word);
        //printf ("Loaded %x at %x\n", word, pc_word);
        ii+=4;
    }
    else {
        printf ("Incomplete PC File\n");
    }
  }

  printf("Read %d words from program into memory.\n\n", ii/4);
}

/************************************************************/
/*                                                          */
/* Procedure : initialize                                   */
/*                                                          */
/* Purpose   : Load machine language program                */ 
/*             and set up initial state of the machine.     */
/*                                                          */
/************************************************************/
void initialize(char *program_filename, int num_prog_files, char *pc_filename) { 
  int i;

  init_memory();
  for ( i = 0; i < num_prog_files; i++ ) {
    printf ("Loading %s\n", program_filename);
    load_program(program_filename, pc_filename);
    while(*program_filename++ != '\0');
  }
  printf ("Init done\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : sim                                             */
/*                                                             */
/***************************************************************/
void sim(char *instr_hex, char *pc_values_hex) {                              
 
  printf("RISC-V Simulator\n\n");

  initialize(instr_hex, 1, pc_values_hex);
}

extern void init (char *test_name) {
  char instr_hex[99];
  char pc_values_hex[99];

  instr_count = 0;

  strcpy (instr_hex, test_name);
  strcpy (pc_values_hex, test_name);

  //printf ("%s\t%s\n", instr_hex, pc_values_hex);
  strcat (instr_hex, (const char*)".hex");
  strcat (pc_values_hex, (const char*)"_pc.hex");

  //printf ("%s\t%s\n", instr_hex, pc_values_hex);
  sim (instr_hex, pc_values_hex);
}

int main (int argc, char* test_name[]) {
    //printf ("Test name is %s\n", test_name[1]);
    init (test_name[1]);
    go();
    return 0;
}
