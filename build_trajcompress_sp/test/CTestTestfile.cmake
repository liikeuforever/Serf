# CMake generated Testfile for 
# Source directory: /Users/xuzihang/GitProject/GG/Serf/test
# Build directory: /Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/chimp_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/deflate_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/elf_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/fpc_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/gorilla_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/lz4_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/lz77_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/machete_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/serf_qt_gps_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/serf_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/sim_piece_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/PerformanceProgram[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_trajcompress_sp/test/PerformanceProgram_ADT[1]_include.cmake")
subdirs("baselines/deflate")
subdirs("baselines/fpc")
subdirs("baselines/lz4")
subdirs("baselines/chimp128")
subdirs("baselines/gorilla")
subdirs("baselines/elf")
subdirs("baselines/machete")
subdirs("baselines/lz77")
subdirs("baselines/sz2")
subdirs("baselines/snappy")
subdirs("baselines/zstd/build/cmake")
subdirs("baselines/sim_piece")
subdirs("baselines/alp")
subdirs("baselines/sz_adt")
subdirs("../_deps/googletest-build")
