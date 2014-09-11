g++ -fpermissive -w -o server server.c ae.c anet.c zmalloc.c
g++ -fpermissive -w -o client client.c ae.c anet.c zmalloc.c
g++ -fpermissive -w -o easysocketbenchmark easysocketbenchmark.cpp ae.c anet.c zmalloc.c -lpthread
