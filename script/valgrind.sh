#valgrind -v --leak-check=yes ./bin/test_mpaxos.out ./config/config.1.1 1 1 1 1 1 2
valgrind -v --leak-check=full --show-reachable=yes ./bin/test_mpaxos.out ./config/config.1.1 1 1 1 1 1 2

