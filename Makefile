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

