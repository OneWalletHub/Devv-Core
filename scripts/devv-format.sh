#!/bin/bash

formatfile=$1

# Check for non-null filename
if [ "x${formatfile}" = "x" ]; then
    echo "usage: devv-format.sh FILENAME"
    exit -1
fi

# Check that file exists
if [ ! -e $formatfile ]; then
    echo "${formatfile} not found, exiting."
    exit -1
fi

# Check for unstaged changes
d=$(git diff --shortstat ${1})
if [ "x$d" != "x" ]; then
    echo "${formatfile} is dirty - please stage or commit changes"
    exit -1
fi
    
tmpfile=`mktemp`

clang-format -style=file ${formatfile} > ${tmpfile}
cp ${tmpfile} ${formatfile}
rm ${tmpfile}

echo "Reformatted ${formatfile}"
