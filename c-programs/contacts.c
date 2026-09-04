#include <stdio.h>
#include <string.h>

struct Contact {
    char name[50];
    char phone[20];
};

void addContact() {
    struct Contact c;
    printf("Enter name: ");
    scanf(" %[^\n]", c.name);
    printf("Enter phone: ");
    scanf(" %[^\n]", c.phone);

    FILE *file = fopen("contacts.txt", "a");
    fprintf(file, "%s,%s\n", c.name, c.phone);
    fclose(file);
    printf("Contact added!\n");
}

void viewContacts() {
    char name[50], phone[20];
    FILE *file = fopen("contacts.txt", "r");
    if (file == NULL) {
        printf("No contacts found.\n");
        return;
    }
    printf("Contact List:\n");
    while (fscanf(file, " %49[^,],%19[^\n]", name, phone) == 2) {
        printf("Name: %s | Phone: %s\n", name, phone);
    }
    fclose(file);
}

int main() {
    int choice;
    printf("1. Add Contact\n2. View Contacts\nChoice: ");
    scanf("%d", &choice);

    if (choice == 1) {
        addContact();
    } else if (choice == 2) {
        viewContacts();
    } else {
        printf("Invalid choice.\n");
    }
    return 0;
}
