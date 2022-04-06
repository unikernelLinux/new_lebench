DIR=$1
STATS=" | datamash mean 1 median 1 pstdev 1 max 1 min 1"
function stats_basic () {
    TEST=$1
    MUNGE=$2

    echo
    echo $TEST
    FILE=$DIR/new_lebench_$TEST.csv
    CMD="cat $FILE $MUNGE $STATS"
    echo $CMD
    eval $CMD
}

MUNGE=" | tail -n +2 | cut -d ',' -f 2 "
MUNGE2=" | tail -n +2 | grep ',1,' | cut -d ',' -f 3 "

MUNGE3=" | tail -n +2 | grep ',10,' | cut -d ',' -f 3 "
MUNGE4=" | tail -n +2 | grep ',4096,' | cut -d ',' -f 3 "

stats_basic clock "$MUNGE"
stats_basic cpu "$MUNGE"
stats_basic getppid "$MUNGE"

stats_basic read  "$MUNGE2"
stats_basic write "$MUNGE2"
stats_basic send  "$MUNGE2"
stats_basic recv  "$MUNGE2"

stats_basic select "$MUNGE3"

stats_basic pagefault      "$MUNGE4"
stats_basic stackpagefault "$MUNGE4"
exit
TEST=select
TEST=pagefault
TEST=stackpagefault
# $CMD  | tail -n +2
# cat $FILE | $MUNGE | $STATS
