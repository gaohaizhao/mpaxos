gprof ./test_mpaxos gmon.out > result.txt
gprof2dot.py result.txt > test.dot
dot test.dot -Tpng -o test.png
