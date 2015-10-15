

all:
	make -C ../../../Soca
	metil_comp -O3 -g3 -I../../../Soca/src -ILMT/include -Isrc/lib_abq -DQT_THREAD_SUPPORT src/main.cpp

LMTGlobalManager:
	make -C ../../../Soca
	metil_comp -O3 -g3 -I../../../Soca/src -ILMT/include -Isrc/lib_abq -DQT_THREAD_SUPPORT src/main.cpp


gdb:
	metil_comp --gdb -O3 -g3 -I../../../Soca/src -ILMT/include src/main.cpp

val:
	metil_comp --valgrind -O3 -g3 -I../../../Soca/src -ILMT/include src/main.cpp

