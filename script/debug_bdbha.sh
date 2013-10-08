


N_HOST=5


TARGET="java -jar ./testbdbha.jar"

killall xterm &> /dev/null
rm -rf ./bdbdir* &> /dev/null

for i in $(seq 1 $N_HOST)
do
    mkdir ./bdbdir$i
done

#java -jar ./testbdbha.jar -env ./bdb1 -nodeName node1 -nodeHost 127.0.0.1:9001 -helperHost 127.0.0.1:9001

for i in $(seq 1 $N_HOST)
do
    command="$TARGET -env ./bdbdir$i -nodeName node$i -nodeHost 127.0.0.1:900$i -helperHost 127.0.0.1:9001"
    nohup xterm -hold -e "$command" &
done

