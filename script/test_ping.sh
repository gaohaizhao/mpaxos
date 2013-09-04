
mkdir result.ping

for i in $(seq ${N_HOST})
do
    for j in $(seq ${i} ${N_HOST})
    do
        command="ping -c 100 ${MHOST[$j]}"
        ssh ${MHOST[$i]} $command > result.ping/result.ping.$i.$j
    done
done

