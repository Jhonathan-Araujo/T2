#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/t2_driver"
#define BUFFER_LENGTH 256

int main() {
    int fd;
    char buffer[BUFFER_LENGTH];
    char command[BUFFER_LENGTH];
    ssize_t ret;
    char process_name[50];

    printf("Starting message queue test application...\n");

    // Open the device with read/write access
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device...");
        return errno;
    }

    // Register the process
    printf("Enter a name to register with the message queue: ");
    fgets(process_name, sizeof(process_name), stdin);
    process_name[strcspn(process_name, "\n")] = '\0'; // Remove newline character

    snprintf(command, BUFFER_LENGTH, "/reg %s", process_name);
    ret = write(fd, command, strlen(command));
    if (ret < 0) {
        perror("Failed to register with the driver.");
        close(fd);
        return errno;
    }
    printf("Registered as '%s'\n", process_name);

    // Main loop
    while (1) {
        int choice;
        printf("\nChoose an action:\n");
        printf("1. Send a message\n");
        printf("2. Read messages\n");
        printf("3. Unregister and exit\n");
        printf("Enter your choice: ");
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            // Clear stdin buffer
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            continue;
        }
        getchar(); // Consume newline character left by scanf

        if (choice == 1) {
            char dest_name[50];
            char message[200];

            printf("Enter the destination process name: ");
            fgets(dest_name, sizeof(dest_name), stdin);
            dest_name[strcspn(dest_name, "\n")] = '\0';

            printf("Enter the message to send: ");
            fgets(message, sizeof(message), stdin);
            message[strcspn(message, "\n")] = '\0';

            snprintf(command, BUFFER_LENGTH, "/%s %s", dest_name, message);
            ret = write(fd, command, strlen(command));
            if (ret < 0) {
                perror("Failed to send the message.");
            } else {
                printf("Message sent to '%s'\n", dest_name);
            }

        } else if (choice == 2) {
            ret = read(fd, buffer, BUFFER_LENGTH - 1);
            if (ret < 0) {
                perror("Failed to read messages.");
            } else if (ret == 0) {
                printf("No messages available.\n");
            } else {
                buffer[ret] = '\0'; // Null-terminate the string
                printf("Received message: '%s'\n", buffer);
            }

        } else if (choice == 3) {
            // Unregister and exit
            snprintf(command, BUFFER_LENGTH, "/unreg %s", process_name);
            ret = write(fd, command, strlen(command));
            if (ret < 0) {
                perror("Failed to unregister.");
            } else {
                printf("Unregistered successfully.\n");
            }
            break;

        } else {
            printf("Invalid choice. Please try again.\n");
        }
    }

    close(fd);
    printf("Exiting application.\n");
    return 0;
}