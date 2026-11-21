# ir.sh BIN FILE ENV... : instructions (callgrind) and size
B=$1; F=$2; shift 2
O=/tmp/cgo.$$; K=/tmp/cgk.$$
T=$(env "$@" valgrind --tool=callgrind --callgrind-out-file=$O $B ${MODE:--m balanced} -T 1 $F $K 2>&1 | awk '/Collected/{print $4}')
printf "%-26s %-50s %8.1fM %9d\n" $(basename $F) "$*" $(echo "$T/1000000" | bc -l) $(stat -c %s $K)
rm -f $O $K
