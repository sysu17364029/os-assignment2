gcc vm.c -o vm -w
echo "256 physical pages with FIFO replacement: "
./vm BACKING_STORE.bin addresses.txt -p FIFO -n 256
echo "256 physical pages with LRU replacement: "
./vm BACKING_STORE.bin addresses.txt -p LRU -n 256
echo "128 physical pages with FIFO replacement: "
./vm BACKING_STORE.bin addresses.txt -p FIFO -n 128
echo "128 physical pages with LRU replacement: "
./vm BACKING_STORE.bin addresses.txt -p LRU -n 128
