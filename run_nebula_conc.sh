#!/bin/bash

# run concurrent queue tests on nebula

ZIP=$1
PROG=$2
TIMES=$3
BATCH_SIZES=$4
THREADS=$5
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

    # all threads enqueing and dequeing with the same batch size
    a() {
      local p=\$1 b=\$2
      yes "\$b" | head -n "\$p" | paste -sd,
    }

    # one thread enqueing, all other threads dequeing with the same batch size
    b() {
      local n=\$1 b=\$2
      local E=() D=()
      for (( i=0; i<n; i++ )); do
        if (( i == 0 )); then
          E+=("\$b"); D+=(0) 
        else
          E+=(0); D+=("\$b") 
        fi
      done
      echo "\$(IFS=,; echo "\${E[*]}")|\$(IFS=,; echo "\${D[*]}")"
    }

    # all threads with id smaller than p/2 enqueing, the other threads dequeing
    c() {
      local n=\$1 b=\$2
      local E=() D=()
      for (( i=0; i<n; i++ )); do
        if (( i < n/2 )); then
          E+=("\$b"); D+=(0) 
        else
          E+=(0); D+=("\$b") 
        fi
      done
      echo "\$(IFS=,; echo "\${E[*]}")|\$(IFS=,; echo "\${D[*]}")" 
    }

    # even numbered threads enqueing, odd numbered threads dequeing
    d() {
      local n=\$1 b=\$2
      local E=() D=()
      for (( i=0; i<n; i++ )); do
        if (( i%2 == 0 )); then
          E+=("\$b"); D+=(0) 
        else
          E+=(0); D+=("\$b") 
        fi
      done
      echo "\$(IFS=,; echo "\${E[*]}")|\$(IFS=,; echo "\${D[*]}")" 
    }

    times=($TIMES)
    threads=($THREADS)
    batch_sizes=($BATCH_SIZES)
    patterns=(a b c d)

    for pat in "\${patterns[@]}"; do
      for n in "\${threads[@]}"; do
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

            logfile="../data/\$(basename $PROG)_n\${n}_t\${t}_b\${b}_\${pat}.log"

            echo "'Running \$logfile'"

            srun -t 2 -p q_student $PROG -n "\$n" -t "\$t" -r 10 -E "\$E" -D "\$D" | tee "\$logfile"

            while [ "\$(squeue -u \$(whoami) | wc -l)" -ne 1 ]; do
              squeue
              sleep 1
            done

          done
        done
      done
    done

    echo 'done'
EOSSH
}

if [ $# -ne 5 ]; then
  echo "USAGE: run_nebula_conc.sh <project.zip> <benchmark executable> <times> <batch sizes> <threads>"
  exit 1
fi

mkdir_neb && cp_to_neb && run_neb && cp_from_neb

