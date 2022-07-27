# ysyx-npc-difftest-version1


readme is here!



ysyx-npc simulation environment, just have difftest, reference is nemu

ysyx-npc-difftest version 1.3


verilator version 4.210

this is a object for ysyx. 
test five stage assembly line, just have difftest, referance is nemu.  
if u have other idea, please debug by yourself.

####################################################################

1.your top moudle is named " top "

top.v interface

	moudle top (
		input wire clk, //clock
		input wire rst, //rstern
	
		//intruction sram			from ifu
		output wire isram_e, // enable instruction sram read 1-enable, 0-disenable
		output wire [63: 0] isram_addr,  // instruction address - pc
		//							send to idu
		input  wire [31: 0] isram_rdata, // instruction
	
		//data sram					from exu
		output wire dsram_e,  // enable data sram, write or read, 1-e,0-d
		output wire dsram_we, // enable data sram, write
	
		//difftest need				 from write back 
		output wire [63: 0] debug_wb_pc, // u can change this wire
		output wire [63: 0] debug_wb_npc, 
		output bubble     // if it is a bubble,"c" will skip difftest once  
	);
		// from exu and send to exu
		wire [63: 0] dsram_addr; //read or wire need data-sram address
		wire [63: 0] dsram_wdata; //data sram write data 
		wire [ 7: 0] dsram_sel;   //dataa sram write size selection
							      // 64bits 1111_1111 32bits 0000_1111 
								  // 16bits 0000_0011 8 bits 0000_0001
		wire [63: 0] dsram_rdata; //read data-sram dara

		//data sram read "DPI-C"  if u have new idea, u will chang here and "C".
		import "DPI-C" function void mem_read(   
			input longint raddr, output longint rdata );
	
		//data sram write "DPI-C"
		import "DPI-C" function void mem_write (
			input longint waddr, input longint wdata, input byte wmask );

		always @(*) begin
			mem_read(dsram_addr, dsram_rdata); 
			//read 64bits data, u need code your exe moudle to cut 8\16\32\64bits
			mem_write(dsram_addr, dsram_wdata, dsram_sel);
		end  

		yourmoudle xxxu(
			.x (y),
			...
		);

	endmoudle

####################################################################

2.maybe your npc don't have ebreak(),
u have to code your am at a-m/am/src/riscv/npc/trm.c.  
code what ? look nemu, copy.

ref_difftest_exec()\ref_difftest_regcpy()\ref_difftest_memcpuy()...
at the same time, u should code them in neme/src/cpt/difftest/ref.c
remeber, regcpy() need to copy pc and special regs.

####################################################################

3.add some code at a-m/scripts/riscv64-npc.mk

 	run: image 	
		make -C $(NPC_HOME) run IMAGE = $(IMAGE).bin

//some times your nemu is bad , so this is a big question.
//phase3'difftest is better, but i don't have. maybe it will be ysyx-npc-difftest-version 2.

####################################################################

4.u have to get riscv64-nemu-interpreter-so

	xxx@xx:~$ cd $NEMU_HOME
	xxx@xx:~/$(NEMU_HOME)$ make menucofig
	option: Build target -> Share object(used as REF for differential testing)
	select -> save -> exit
	xxx@xx:~/$(NEMU_HOME)$ make run

u will get riscv64-nemu-interpreter-so at $(NEMU_HOME)/build
otherwise, u need to code na.cpp line:226 

####################################################################

5.use ebreak() in wbu

	import "DPI-C" function void ebreak;
	always @(*) begin
		if(this instruction is ebreak) bgein
			ebreak();
		end
	end

#####################################################################

6.your NPC must handle the result of mem_read->u have to use mask for data_sram result

    //exu load and store example , u get mask in mem stage
    wire [ 7: 0] ex_dsram_sel;
    assign dsram_e  =   dram_e;
    assign dsram_we =   dram_we;
    assign dsram_addr   =   ex_result;

    assign dsram_wdata  =   inst_sb ? {  8{rf_rdata2[ 7: 0]} } :
                            inst_sh ? {  4{rf_rdata2[15: 0]} } :
                            inst_sw ? {  2{rf_rdata2[31: 0]} } :
                            inst_sd ? rf_rdata2 : 64'b0;

    assign ex_dsram_sel    =    lsu_64  ? 8'b1111_1111 :
                                lsu_32  ? { {4{byte_sel[4]}}, {4{byte_sel[0]}}} :
                                lsu_16  ? { {2{byte_sel[6]}}, {2{byte_sel[4]}}, {2{byte_sel[2]}}, {2{byte_sel[0]}} } : 
                                lsu_8   ? byte_sel  : 8'b0;

    assign dsram_sel = ex_dsram_sel;

#####################################################################

7.u need add printf in pmemm_read() to get mtrace,.

u must understand mem_read & mem_write how to deak with address and data.

remmeber, mem_read usu in exu(mine npc), your cpuu.pc or pc is wbu_pc, so u need add a new wire from exu to top.


readme over
that's all for now(2022/7/26), i will add later when i think of it.

