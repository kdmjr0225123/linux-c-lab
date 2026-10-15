#!/bin/bash
# Usage: ./run_lab6.sh N   (N = exercise 1-6). Compiles and runs one exercise.
cd "$(dirname "$0")"
case "$1" in
  1) cmd="gcc -o producer_consumer producer_consumer.c -pthread";             run=./producer_consumer ;;
  2) cmd="g++ -o bank_account bank_account.cpp -pthread -std=c++11";          run=./bank_account ;;
  3) cmd="gcc -o reader_writer reader_writer.c -pthread";                     run=./reader_writer ;;
  4) cmd="g++ -o dining_philosophers dining_philosophers.cpp -pthread -std=c++11"; run=./dining_philosophers ;;
  5) cmd="gcc -o sleeping_barber sleeping_barber.c -pthread";                 run=./sleeping_barber ;;
  6) cmd="g++ -o bounded_buffer bounded_buffer.cpp -pthread -std=c++11";      run=./bounded_buffer ;;
  *) echo "usage: $0 1-6"; exit 1 ;;
esac
clear
echo "\$ $cmd"; $cmd || exit 1
echo "\$ $run"; $run
