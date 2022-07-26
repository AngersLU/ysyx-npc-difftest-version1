#define Vname V##top
#include "Vtop.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <getopt.h>
#include "difftest-def.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "svdpi.h"
#include "Vtop__Dpi.h"
#include "verilated_dpi.h"

#define CONFIG_MBASE 0x80000000
#define CONFIG_MSIZE 0x8000000

typedef uint64_t paddr_t;

uint64_t pmem_read(paddr_t addr, int len);
void pmem_write(paddr_t addr, int len, uint64_t data);

long load_image(char *filename);
void difftest_step(paddr_t pc, paddr_t npc);
void init_difftest(char *ref_so_file, long img_size, int port);

VerilatedContext *contextp = NULL;
VerilatedVcdC *tfp = NULL;
static Vname *top;
static vluint64_t main_time = 0;
static const vluint64_t sim_time = 1000;

using namespace std;
bool is_exit = false;

struct CPU_state
{
  uint64_t gpr[32];
  uint64_t pc;
}cpuu;


// DPI-C
bool isebreak = false;
extern "C" void ebreak() {
  isebreak = true;
}

uint64_t *cpu_gpr = NULL;
extern "C" void set_gpr_ptr(const svOpenArrayHandle r) {
  cpu_gpr = (uint64_t *)(((VerilatedDpiOpenVar *)r)->datap());
  //cpuu.pc = top->pc;
}


// DPI-C
extern "C" void mem_read(long long raddr, long long *rdata) { 
  if(raddr<0x88000000 && raddr > 0x80000000 ){
    if( top->dsram_e && !top->dsram_we ){
    // 总是读取地址为`raddr & ~0x7ull`的8字节返回给`rdata`
     // pmem_read(      *(uint64_t *)(raddr & ~0x7ull) ;//;
      *rdata = pmem_read((raddr & ~0x7ull), 8);
    }
  }
}

extern "C" void mem_write(long long waddr, long long wdata, char wmask) {
  // 总是往地址为`waddr & ~0x7ull`的8字节按写掩码`wmask`写入`wdata`
  // `wmask`中每比特表示`wdata`中1个字节的掩码,
  // 如`wmask = 0x3`代表只写入最低2个字节, 内存中的其它字节保持不变
  if(waddr<0x88000000 && waddr > 0x80000000 ){
    if( top->dsram_e && top->dsram_we ){
      long long mask = 0;
      for(int i = 0; i < 8; i++){
        if((wmask >> i) & 0x01){
          long long f = 0xff; 
          f = f << (i * 8);
          mask |= f;
        }
      }
      mask = ~mask;
      long long wdata_z = wdata | (pmem_read((waddr & ~0x7ull), 8) & mask);
      pmem_write((waddr & ~0x7ull), 8, wdata_z);
    }
  }
}


extern "C" void pc_print(long long pc) {
  printf("pc_reg:0x%08llx\n", pc);
}

//pc_print use example
// import "DPI-C" function void pc_print(input longint pc);

// always @(posedge clk) begin
//     pc_print({61'b0, stallreq_for_bru, stallreq_for_ex, stallreq_for_load});
// end


//////////////////////////////////////////////////////////////////////////////////////////////////////////
// read .bin
static char *img_file = NULL;
static int parse_args(int argc, char *argv[]) {
  const struct option table[] = {};
  int o;
  while ((o = getopt_long(argc, argv, "-bhl:d:p:", table, NULL)) != -1)
  {
    switch (o)
    {
    case 1:
    {
      img_file = optarg;
      return 0;
    }
    default:
      exit(0);
    }
  }
  return 0;
}

void sim_exit() {
  tfp->close();
  delete top;
  delete contextp;
}

void exit_now() {
  is_exit = true;
}

void sim_init() {
  contextp = new VerilatedContext;
  contextp->traceEverOn(true);
  top = new Vtop;
  tfp = new VerilatedVcdC;

  top->trace(tfp, 99);
  tfp->open("wave.vcd");
}

// CMD
void isa_reg_display() {
	for(int i=0; i<32; i=i+2){				//64->16,4 but 8 seem looks better
		printf("%s\t0x%08lx\t", regs[i], cpuu.gpr[i]);
		printf("%s\t0x%08lx\n", regs[i+1], cpuu.gpr[i+1]);
	}
	printf("pc\t0x%08lx\n", cpuu.pc);

}

static uint64_t refpc = 0;
static uint64_t pc;
static uint64_t npc;
bool bubble;
static int cmd_c()
{
  while (!contextp->gotFinish()) //&& main_time < sim_time) 
  { 
    if(main_time < 10) {
      top->rst = 1; top->eval();
    }
    if(main_time >= 10) {
      top->rst = 0; top->eval();
      
      if (main_time % 10 == 1) {
        //printf("pc:0x%lx, instr:0x%08lx\n", top->pc, pmem_read(top->pc, 4));
        if(top->clk == 1) {
          if (top->isram_e == 1) {
            // printf("top->isram_addr: %08lx \n", top->isram_addr);
            top->isram_rdata = pmem_read(top->isram_addr, 4);
          }
          //printf("pc:0x%lx, bubble:0x%08lx\n", top->difftest_pc, top->bubble);
          if(main_time >= 60) {
            if(pc >= CONFIG_MBASE && pc <= (CONFIG_MBASE + CONFIG_MSIZE) ) {
              for(int i = 0; i < 32; i++) cpuu.gpr[i] = cpu_gpr[i];
              // sp regs are used for addtion
              if(bubble != 1) {
                printf("     pc 0x%08lx \n", pc);
                difftest_step(pc, npc);
              }
            }
          }
          pc = top->debug_wb_pc;
          npc = top->debug_wb_npc;
          bubble = top->bubble;
          cpuu.pc = pc;
        }
      }
    }

    if (isebreak || is_exit) {
      if(isebreak)
        printf("\033[1;32mebreak \33[0m\n");
      break;
    }

    if(main_time % 10 == 0) {
      top->clk = 1;
    }
    if(main_time % 10 == 5) {
      top->clk = 0;
    }
    contextp->timeInc(1);
    top->eval();
    tfp->dump(main_time);
    main_time++;
    // printf("top->debug_if_pc: %08lx\n", top->debug_if_pc);
  }

  return 0;
}

char str[10];

int main(int argc, char **argv)
{
  parse_args(argc, argv);
  long img_size = load_image(img_file);
  printf("\033[1;31mWelcome to fxxk NPC\033[0m\n");
  printf("\033[1;32mimg_size %lx\33[0m\n", img_size);

  char str[] = "/home/lff/ysyx-workbench/nemu/build/riscv64-nemu-interpreter-so";
  static char *diff_so_file = str;
  static int difftest_port = 1234;
  init_difftest(diff_so_file, img_size, difftest_port);
  Verilated::commandArgs(argc, argv);
  sim_init();

  while (1) {
    cmd_c();
    if (isebreak || is_exit)  {
      if(cpuu.gpr[10] != 0) printf("\033[1;31mHIT BAD TRAP\33[0m\n");
      else  printf("\033[1;32mHIT GOOD TRAP\33[0m\n");
      break;
    }
    else {
      printf("\033[1;31mLOOP!\33[0m\n");
      assert(0);
    }
    
  }

  sim_exit();
  return 0;
}
