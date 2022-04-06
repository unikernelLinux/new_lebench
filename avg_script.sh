awk '{ total += $1 } END { print total/NR }' <( cat $1 | cut -d , -f 2 )
