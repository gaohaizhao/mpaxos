
source hosts.sh 

mkdir result.network

# start netperf server
for i in $(seq 5)
do
    echo ${MHOST[$i]}
    ssh ${MHOST[$i]} "netserver" 
done

# start test each one
for i in $(seq 5)
do
    for j in $(seq ${i} 5)
    do
        command="netperf -H ${MHOST[$j]} -l 5"
        ssh ${MHOST[$i]} $command > result.network/result.bandwidth.$i.$j
        command="ping -c 10 ${MHOST[$j]}"
        ssh ${MHOST[$i]} $command > result.network/result.latency.$i.$j
    done
done
