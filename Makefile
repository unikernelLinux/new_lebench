.PHONY: new_lebench

#PATHS
DIR := ${CURDIR}/..
GCC_LIB=$(DIR)/gcc-build/x86_64-pc-linux-gnu/libgcc/
LC_DIR=$(DIR)/glibc-build/
CRT_LIB=$(LC_DIR)csu/
C_LIB=$(LC_DIR)libc.a
PTHREAD_LIB=$(LC_DIR)nptl/libpthread.a
RT_LIB=$(LC_DIR)rt/librt.a
MATH_LIB=$(LC_DIR)math/libm.a
CRT_STARTS=$(CRT_LIB)crt1.o $(CRT_LIB)crti.o $(GCC_LIB)crtbeginT.o
CRT_ENDS=$(GCC_LIB)crtend.o $(CRT_LIB)crtn.o
SYS_LIBS=$(GCC_LIB)libgcc.a $(GCC_LIB)libgcc_eh.a

UKL_FLAGS=-ggdb -mno-red-zone -mcmodel=kernel

#-----------------------------------------------------------------------------
#-----------------------------------------------------------------------------

#MYBENCH_SMALL
new_lebench:
	- rm -rf UKL.a new_lebench.o 
	gcc -c -o new_lebench.o new_lebench.c $(UKL_FLAGS) \
		-UUSE_VMALLOC -UBYPASS -UUSE_MALLOC \
                -DREF_TEST -USEND_TEST -URECV_TEST -DTHREAD_TEST \
                -UFORK_TEST -DWRITE_TEST -DREAD_TEST -DPF_TEST -DST_PF_TEST \
                -UDEBUG
	ld -r -o new_lebench.ukl --allow-multiple-definition $(CRT_STARTS) new_lebench.o \
                --start-group --whole-archive  $(PTHREAD_LIB) \
                $(C_LIB) --no-whole-archive $(SYS_LIBS) --end-group $(CRT_ENDS)
	ar cr UKL.a new_lebench.ukl ../undefined_sys_hack.o
	objcopy --prefix-symbols=ukl_ UKL.a
	objcopy --redefine-syms=../redef_sym_names UKL.a
	cp UKL.a ../


SYM_TESTS=-DREF_TEST -DTHREAD_TEST -DFORK_TEST -DSEND_TEST -DRECV_TEST -DWRITE_TEST -DREAD_TEST -DPF_TEST -DST_PF_TEST -DSELECT_TEST -DCTX_SW_TEST
SYM_CONFIG=-UUSE_VMALLOC -UBYPASS -DUSE_MALLOC -DSYM_ELEVATE
SYM_DEBUG=-UDEBUG
SYM_SYS_LIBS=-pthread
SYMBI=../Apps/libs/symlib/build/libsym.a ../Apps/libs/kallsymlib/libkallsym.a -I ../Apps/libs/symlib/include

# lazy
sym: sym_lebench
sym_lebench: new_lebench.c
	gcc $< -o new_lebench $(SYM_SYS_LIBS) $(SYM_CONFIG) $(SYM_TESTS) $(SYM_DEBUG) $(SYMBI)

sym_interpose_cores:
# Core 0
	cd ../Apps/bin/recipes/ && ./interposing_mitigator.sh -m tf -t 0 -d
	cd ../Apps/bin/recipes/ && ./interposing_mitigator.sh -m df -t 0 -d
# Core 1
	cd ../Apps/bin/recipes/ && ./interposing_mitigator.sh -m tf -t 1 -d
	cd ../Apps/bin/recipes/ && ./interposing_mitigator.sh -m df -t 1 -d

sym_mv_csvs:
	mkdir -p output
	mv *.csv output

sym_clean:
	rm -rf sym_lebench new_lebench new_lebench.o
	rm -rf *.csv
	rm -rf test_file.txt

sym_clean_all: sym_clean
	rm -rf output
