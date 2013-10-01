


N_HOST=5


TARGET=../bin/test_bdb.out

killall xterm &> /dev/null
rm -rf ~/bdbdir* &> /dev/null

for i in $(seq 1 $N_HOST)
do
    mkdir ~/bdbdir$i
done


for i in $(seq 1 $N_HOST)
do
    command="$TARGET -h /home/ms/bdbdir$i -l127.0.0.1:700$i -r127.0.0.1:7001 -p0"
    if [ $i -eq 1 ]; then
        command="$TARGET -h /home/ms/bdbdir$i -L127.0.0.1:7001 -p1000;"
    fi
    nohup xterm -hold -e "$command" &
    if [ $i -eq 1 ]; then
        sleep 1
    fi
done

