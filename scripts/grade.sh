#!/bin/bash

WORKDIR=$(dirname "$(dirname "$(readlink -f "$0")")")

build() {
  build_target=$1
  cd "$WORKDIR" && mkdir -p build && cd build && cmake .. >/dev/null && make "$build_target" -j >/dev/null
  if [[ $? != 0 ]]; then
    echo "Error: Compile error, try to run make build and debug"
    exit 1
  fi
}

test_genEasy() {
  local score_str="GenEasy SCORE"
  local ref_dir=${WORKDIR}/testdata/genEasy
  local out_dir=${WORKDIR}/out/genEasy
  
  mkdir -p ${out_dir}
  build genEasy
  ./genEasy >& ${out_dir}/easy.ll
  if [[ $? != 0 ]]; then
      echo "Error: genEasy failed"
      echo "${score_str}: 0"
      exit 1
  fi
  llvm-as ${out_dir}/easy.ll -o ${out_dir}/easy.bc
  if [[ $? != 0 ]]; then
      echo "Error: llvm-as failed"
      echo "${score_str}: 0"
      exit 1
  fi

  value=$(cat ${ref_dir}/1.out)
  lli ${out_dir}/easy.bc > /tmp/output.txt
  if [[ $? != $value ]]; then
    echo "Error: value not correct"
    echo "${score_str}: 0"
    exit 1
  fi

  echo "[^_^]: Pass"
  echo "${score_str}: 100"
}

test_genCalculateDistance() {
  local score_str="GenCalculateDistance SCORE"
  local ref_dir=${WORKDIR}/testdata/genCalculateDistance
  local out_dir=${WORKDIR}/out/genCalculateDistance
  
  mkdir -p ${out_dir}
  build genCalculateDistance
  ./genCalculateDistance >& ${out_dir}/calculateDistance.ll
  if [[ $? != 0 ]]; then
      echo "Error: genCalculateDistance failed"
      echo "${score_str}: 0"
      exit 1
  fi
  llvm-as ${out_dir}/calculateDistance.ll -o ${out_dir}/calculateDistance.bc
  if [[ $? != 0 ]]; then
      echo "Error: llvm-as failed"
      echo "${score_str}: 0"
      exit 1
  fi
  cat ${ref_dir}/1.in | lli ${out_dir}/calculateDistance.bc > /tmp/output.txt
  diff -w /tmp/output.txt ${ref_dir}/1.out
  if [[ $? != 0 ]]; then
    echo "Error: Output mismatch"
    echo "${score_str}: 0"
    exit 1
  fi
  cat ${ref_dir}/2.in | lli ${out_dir}/calculateDistance.bc > /tmp/output.txt
  diff -w /tmp/output.txt ${ref_dir}/2.out
  if [[ $? != 0 ]]; then
    echo "Error: Output mismatch"
    echo "${score_str}: 0"
    exit 1
  fi

  echo "[^_^]: Pass"
  echo "${score_str}: 100"
}

main() {
  local scope=$1

  if [[ ! $(pwd) == "$WORKDIR" ]]; then
    echo "Error: Please run this grading script in the root dir of the project"
    exit 1
  fi

  if [[ ! $(uname -s) == "Linux" ]]; then
    echo "Error: Please run this grading script in a Linux system"
    exit 1
  fi

  if [[ $scope == "genEasy" ]]; then
    echo "========== GenEasy Test =========="
    test_genEasy
  elif [[ $scope == "genCalculateDistance" ]]; then
    echo "========== GenCalculateDistance Test =========="
    test_genCalculateDistance
  else
    echo "Wrong test scope: Please specify the part you want to test"
    echo -e "\tscripts/grade.sh [genEasy|genCalculateDistance]"
    echo -e "or"
    echo -e "\tmake [genEasy|genCalculateDistance]"
  fi
}

main "$1"