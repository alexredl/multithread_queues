#!/bin/bash

# just becuase project skelleton requires this thing

ZIP=$1
TARGET=$2
LOG=nebula.log
DIR_LOC=data
DIR_NEB=hubble

cp_to_neb() {
  scp $ZIP nebula:~/$DIR_NEB/$ZIP
}

cp_from_neb() {
  mkdir -p $DIR_LOC
  scp -r "nebula:~/$DIR_NEB/data/*" $DIR_NEB
}

mkdir_neb() {
  ssh nebula "rm -rf $DIR_NEB"
  ssh nebula "mkdir $DIR_NEB"
}

run_neb() {
  date | tee -a $LOG
  ssh nebula "
    cd $DIR_NEB
    unzip -u $ZIP
    make

    srun -t 1 -p q_student make $TARGET
    while "'[ $(squeue -u $(whoami) | wc -l) -ne 1 ]'"; do
      squeue
      sleep 1
    done

    echo 'done'
  " | tee -a $LOG
}

if [ $# -ne 2 ]; then
  echo "USAGE: run_nebula.sh <project.zip> <make target, e.g. small-bench>"
  exit 1
fi

mkdir_neb && cp_to_neb && run_neb && cp_from_neb

