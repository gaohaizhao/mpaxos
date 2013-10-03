#gprof ./test_mpaxos gmon.out > result.txt
gprof "../bin/test_mpaxos.out ../config/config.1.1 1 10 10000 1 1 3" gmon.out > result.txt

gprof2dot.py result.txt > test.dot
dot test.dot -Tpng -o test.png
