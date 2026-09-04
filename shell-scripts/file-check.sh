#!/bin/bash
read -p "Enter a filename to check: " FILE
if [ -f "$FILE" ]; then
    echo "File exists!"
    ls -l "$FILE"
else
    echo "File does not exist."
fi
