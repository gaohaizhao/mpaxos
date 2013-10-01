source hosts.sh
rm -rf result.network
mkdir result.network

N_PING=10
# start netperf server
#for i in $(seq 5)
#do
#    echo ${MHOST[$i]}
#    ssh ${MHOST[$i]} "netserver" 
#done

for i in $(seq ${N_HOST})
do
    for j in $(seq ${i} ${N_HOST})
    do
        command="ping -c $N_PING ${MHOST[$j]}"
        ssh $USER@${MHOST[$i]} $command > result.network/result.ping.$i.$j
        
        command="netperf -H ${MHOST[$j]} -l 5"
        ssh $USER@${MHOST[$i]} $command > result.network/result.netperf.$i.$j
    done
done

