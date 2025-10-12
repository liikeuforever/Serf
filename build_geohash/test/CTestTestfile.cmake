# CMake generated Testfile for 
# Source directory: /Users/xuzihang/GitProject/GG/Serf/test
# Build directory: /Users/xuzihang/GitProject/GG/Serf/build_geohash/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/chimp_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/deflate_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/elf_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/fpc_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/gorilla_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/lz4_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/lz77_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/machete_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/serf_qt_gps_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/serf_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/sim_piece_test[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/PerformanceProgram[1]_include.cmake")
include("/Users/xuzihang/GitProject/GG/Serf/build_geohash/test/PerformanceProgram_ADT[1]_include.cmake")
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
