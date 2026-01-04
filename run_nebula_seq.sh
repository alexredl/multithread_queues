#!/bin/bash

# run sequential queue tests on nebula

ZIP=$1
PROG=$2
LOG=nebula.log
DIR_LOC=data
DIR_NEB=hubble

cp_to_neb() {
  scp $ZIP nebula:~/$DIR_NEB/$ZIP
}

cp_from_neb() {
  mkdir -p $DIR_LOC
  scp -r "nebula:~/$DIR_NEB/data/*" $DIR_LOC
}

mkdir_neb() {
  ssh nebula "rm -rf $DIR_NEB"
  ssh nebula "mkdir $DIR_NEB"
}

run_neb() {
  date | tee -a $LOG
  ssh nebula bash -s <<EOSSH | tee -a $LOG
    cd $DIR_NEB
    unzip -u $ZIP
    make
    cd build

    times=(1 5)
    batch_sizes=(1 1000)

    for b in "\${batch_sizes[@]}"; do
      for t in "\${times[@]}"; do

        if [ "\$pat" = "a" ]; then
          E=\$(a "\$n" "\$b")
          D=\$(a "\$n" "\$b")
        else
          tmp=\$(b "\$n" "\$b")
          E=\${tmp%%|*}
          D=\${tmp##*|}
        fi

        logfile="../data/\$(basename $PROG)_t\${t}_b\${b}.log"

        echo "'Running \$logfile'"

        srun -t 2 -p q_student $PROG -n 1 -t "\$t" -r 10 -e "\$b" -d "\$b" | tee "\$logfile"

        while [ "\$(squeue -u \$(whoami) | wc -l)" -ne 1 ]; do
          squeue
          sleep 1
        done

      done
    done

    echo 'done'
EOSSH
}

if [ $# -ne 2 ]; then
  echo "USAGE: run_nebula_seq.sh <project.zip> <benchmark executable>"
  exit 1
fi

mkdir_neb && cp_to_neb && run_neb && cp_from_neb

