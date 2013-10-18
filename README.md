MPaxos
=============

What is MPaxos?

MPaxos refers to two things. 1) MPaxos is an extentional protocol based on Lamport's Paxos. 2) MPaxos is an RSM (Replication State Machine) framework based on the protocol.


=============

What can I do with MPaxos?

MPaxos is used for build for highly reliable distributed services.

1). You can use MPaxos to build a standard Paxos-based RSM, which can tolerate any minority of fail-stops failures.

2). It is also easy to use MPaxos to build independent RSMs running in Parallel. And more importantly, MPaxos supports transactional commits to these RSMs. This is our extentional part to original Paxos.


=============

What interfaces does MPaxos provide?

MPaxos provides an simple Write-Ahead-Log (WAL) and callback interface. To build a reliable application, you only need to abstract the operations of application into logs, commit it using MPaxos. After successful a commit, MPaxos will invoke a callback function (on every node), with the committed log as a parameter.
