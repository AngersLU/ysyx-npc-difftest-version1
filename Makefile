
VSRCS = $(shell find $(abspath $(NPC_HOME)/vsrc) -name "*.v")
CSRCS = $(shell find $(abspath $(NPC_HOME)/csrc) -name "*.cpp" -or -name "*.cc") 
HSRCS += $(shell find $(abspath $(NPC_HOME)/csrc) -name "*.h")

LLVM_CXXFLAGS = $(shell llvm-config-11 --cxxflags)
LLVM_LIBS = $(shell llvm-config-11 --libs)

TOP_NAME = top
VDIR = $(NPC_HOME)/vsrc
CDIR = $(NPC_HOME)/csrc
IMG ?=
ARGS ?=

BUILD_DIR = $(NPC_HOME)/build
OBJ_DIR = $(BUILD_DIR)/obj_dir

BIN = $(BUILD_DIR)/$(TOP_NAME)

default: $(BIN)
	$(shell mkdir -p $(BUILD_DIR))

#rules for verilator
VERILATOR_FLAGS += -MMD --build -cc --exe --trace \
	-O3 --x-assign fast --x-initial fast --noassert \
	--timescale "1ns/1ns"  

INC_PATH = $(NPC_HOME)/vsrc
INCFLAGS = $(addprefix -I , $(INC_PATH))
VCFLAGS += $(INCFLAGS) -DTOP_NAME="\"V$(TOP_NAME)\""
LDFLAGS += -ldl -lSDL2 -lSDL2_image

sim:$(VSRCS) $(CSRCS)
	@rm -rf $(BUILD_DIR)
	verilator $(VERILATOR_FLAGS) --top-module $(TOP_NAME) \
	-I$(VDIR) $(VSRCS) $(CSRCS) $(addprefix -LDFLAGS , $(LLVM_LIBS)) \
	$(addprefix -LDFLAGS , $(LLVM_CXXFLAGS)) -LDFLAGS -ldl



$(BIN): $(VSRCS) $(CSRCS) $(HSRCS)
	@rm -rf $(OBJ_DIR)
	verilator $(VERILATOR_FLAGS) \
		--top-module $(TOP_NAME) $(VSRCS) $(CSRCS) \
		$(addprefix -CFLAGS , $(VCFLAGS)) $(addprefix -LDFLAGS , $(LDFLAGS)) \
		--Mdir $(OBJ_DIR) -I$(VDIR) --exe -o $(abspath $(BIN))
	

run: $(BIN) $(IMAGE) $(ARGS)
	@$^
	$(call git_commit, "npc simlation")
	#@gtkwave $(NPC_HOME)/wave.vcd

git:
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!

clean:
	rm -rf $(OBJ_DIR)

.PHONY:clean
include ../Makefile
