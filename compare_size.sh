#!/bin/bash

set -e

min_column_width=19

if ! [ -n "$size_info_file" ]; then
    size_info_file=size_info.txt
    echo "No file with size info specified! Using default ($size_info_file)"
fi

if ! [ -n "$size_db_file" ]; then
    size_db_file=size_db.txt
    echo "No file with size DB specified! Using default ($size_db_file)"
fi

if ! [ -f "$size_info_file" ]; then
    echo "No file with build sizes ($size_info_file)! Exit."
    exit 0
fi

if ! [ -f "$size_db_file" ]; then
    echo "No DB file with build sizes ($size_db_file)! Exit."
    exit 0
fi


# print the same string multiple times
repl() { printf -- "$1"'%.s' $(seq 1 $2); }

declare -A curr_size_array;
declare -A size_db_array;
declare -a commits_since_master;
GIT_HASH_CUR=`git rev-parse HEAD`
GIT_HASH_MASTER=`git rev-parse origin/master`

if ! [ -n "$GIT_HASH_CUR" ]; then
    echo "Unable to get hash of a current commit"
    exit 1
fi

if [ "$GIT_HASH_CUR" = "$GIT_HASH_MASTER" ]; then
    echo "In master"
    exit 0
fi

#echo "Read size info file"
while read git_hash defconfig_name text data bss dec hex filename
do
    if [ "$git_hash" = "$GIT_HASH_CUR" ]; then
	curr_size_array[$defconfig_name]=$dec
    fi
done < "$size_info_file"

#echo "Read size db file"
while read git_hash defconfig_name text data bss dec hex filename
do
    #echo "$git_hash $defconfig_name $dec"
    size_db_array["$git_hash"_"$defconfig_name"]="$dec"
done < "$size_db_file"

#print header
for i in "${!curr_size_array[@]}"
do
    echo -n "+--"
    # find minimum width
    if [ $min_column_width -gt ${#i} ]; then
	width=$min_column_width
    else
	width=${#i}
    fi
    repl - $width
done
echo "+------------------------------------------------"

for i in "${!curr_size_array[@]}"
do
    # find minimum width
    if [ $min_column_width -gt ${#i} ]; then
	width=$min_column_width
    else
	width=${#i}
    fi
    printf "| %*s " $width $i
done
echo "|"

for i in "${!curr_size_array[@]}"
do
    echo -n "+--"
    # find minimum width
    if [ $min_column_width -gt ${#i} ]; then
	width=$min_column_width
    else
	width=${#i}
    fi
    repl - $width
done
echo "+------------------------------------------------"

# print data
# print current size
for i in "${!curr_size_array[@]}"
do
    # find minimum width
    size=${curr_size_array[$i]}
    if [ $min_column_width -gt ${#i} ]; then
	width=$min_column_width
    else
	width=${#i}
    fi
    printf "| %*s " $width "$size"
done
echo -n "| CURRENT "
# print hash and the title for current commit
git_current_commit=`git log --format=tformat:"%h %s" -1`
echo $git_current_commit

# print info abous previous commits
# pick ! as the separator
# tformat to get the newline after the last entry
git log --format=tformat:"!%H!%s" origin/master~1...HEAD --graph \
| tail -n +2 \
| while IFS="!" read -r git_graph git_hash git_title
do
    for i in "${!curr_size_array[@]}"
    do
	size=${size_db_array["$git_hash"_"$i"]}
	if [ -z "$size" ]; then
	    print_buff=""
	else
	    print_buff="($(($size-${curr_size_array[$i]}))) $size"
	fi

	# find minimum width
	if [ $min_column_width -gt ${#i} ]; then
	    width=$min_column_width
	else
	    width=${#i}
	fi
	printf "| %*s " $width "$print_buff"
    done
    # print graph, hash and title
    printf "|   %-*s %.8s %s\n" 5 "$git_graph" "$git_hash" "$git_title"
done

for i in "${!curr_size_array[@]}"
do
    echo -n "+--"
    # find minimum width
    if [ $min_column_width -gt ${#i} ]; then
	width=$min_column_width
    else
	width=${#i}
    fi
    repl - $width
done
echo "+------------------------------------------------"
