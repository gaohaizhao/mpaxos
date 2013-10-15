#!/bin/bash

source hosts.sh

PEM[1]=mpaxos-tokyo.pem
PEM[2]=mpaxos-singapore.pem
PEM[3]=mpaxos-sydney.pem
PEM[4]=mpaxos-ireland.pem
PEM[5]=mpaxos-california.pem

for i in $(seq $N_HOST)
do
    com="sudo sed -i 's/^PasswordAuthentication.*/PasswordAuthentication yes/' /etc/ssh/sshd_config"
    ssh -o "StrictHostKeyChecking no" -i ${PEM[$i]} $USER@${MHOST[$i]} "$com"
    com="sudo sed -i '/raring multiverse/ s/^#//' /etc/apt/sources.list"
    ssh -o "StrictHostKeyChecking no" -i ${PEM[$i]} $USER@${MHOST[$i]} "$com"
    com="sudo /etc/init.d/ssh restart"
    ssh -o "StrictHostKeyChecking no" -i ${PEM[$i]} $USER@${MHOST[$i]} "$com"
    com="echo -e 'hpcgrid\nhpcgrid' | sudo passwd"
    ssh -o "StrictHostKeyChecking no" -i ${PEM[$i]} $USER@${MHOST[$i]} "$com"
    com="sudo apt-get install libjson0 libapr1 libaprutil1 libprotobuf-c0 netperf openjdk-7-jdk -y"
    ssh -o "StrictHostKeyChecking no" -i ${PEM[$i]} $USER@${MHOST[$i]} "$com"
    cat ~/.ssh/id_rsa.pub | ssh -i ${PEM[$i]} $USER@${MHOST[$i]} "mkdir .ssh; cat >> .ssh/authorized_keys"
    com="echo 'ulimit -c unlimited' >> ~/.bashrc"
    ssh -o "StrictHostKeyChecking no" -i ${PEM[$i]} $USER@${MHOST[$i]} "$com"
done

# Turn on password authentication
#sudo sed -i 's/^PasswordAuthentication.*/PasswordAuthentication yes/' /etc/ssh/sshd_config
# Reload SSHd configuration
# sudo /etc/init.d/ssh restart
# Set a password for the ec2-user
# sudo passwd ubuntu


# sudo apt-get update
# sudo apt-get upgrade -y
# sudo apt-get install libjson0 libapr1 libaprutil1 libprotobuf-c0 netperf -y
