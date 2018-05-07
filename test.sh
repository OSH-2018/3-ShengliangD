#!/bin/bash

tmp_dir=`mktemp -d`
mount_dir=`mktemp -d`

# Mount
init_test() {
    ./sffs $mount_dir
}

# Umount
finish_test() {
    sudo umount $mount_dir
}

# Test basic operations, i.e. open, read, write, unlink
test_basic() {
    tmp_file=`mktemp -p $tmp_dir`
    tfile=$mount_dir`basename $tmp_file`

    dd if=/dev/urandom of=$tmp_file bs=32K count=1 2>/dev/null
    cp $tmp_file $tfile
    diff $tmp_file $tfile

    ret=$?
    [ ! $? ] && echo "Failed: test_basic"

    rm $tmp_file
    rm $tfile

    return $ret
}

test_big_file() {
    tmp_file=`mktemp -p $tmp_dir`
    tfile=$mount_dir`basename $tmp_file`

    dd if=/dev/urandom of=$tmp_file bs=1M count=1 2>/dev/null
    cp $tmp_file $tfile
    diff $tmp_file $tfile

    ret=$?
    [ ! $? ] && echo "Failed: test_big_file"

    rm $tmp_file $tfile

    return $ret
}

test_many_file() {
    for _ in `seq 1024`
    do
        mktemp -p $tmp_dir >/dev/null
    done

    cp $tmp_dir/* $mount_dir

    ls1=`ls $tmp_dir`
    ls2=`ls $mount_dir`
    [ "$ls1" = "$ls2" ]
    ret=$?
    [ ! $ret ] && echo "Failed: test_many_file"

    rm $tmp_dir/* $mount_dir/*

    return $ret
}

init_test && test_basic && test_big_file && test_many_file && finish_test

# TODO: remove temporary dir