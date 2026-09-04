#!/bin/bash
gcc ../c-programs/contacts.c -o contacts
./contacts
tar -czf contacts-backup.tar.gz ../c-programs/*
echo "Output logged at $(date)" >> project-log.txt
