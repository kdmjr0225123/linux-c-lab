#!/bin/bash
read -p "Enter a directory to scan: " DIR
echo "File counts by extension in $DIR:" > file-count-results.txt
for ext in txt sh c; do
    count=$(find "$DIR" -name "*.$ext" 2>/dev/null | wc -l)
    echo "$ext: $count" >> file-count-results.txt
done
cat file-count-results.txt
