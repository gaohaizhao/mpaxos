


N_HOST=5


rm -rf jpaxosLogs &> /dev/null
killall xterm &> /dev/null

for i in $(seq 0 4)
do
    command="java -jar ./testjpaxos.jar $i 5"
    nohup xterm -hold -e "$command" &
done

