#include <stdio.h>

int main() {
    FILE *file = fopen("very_important_file.txt", "w");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    fprintf(file, "I’m a malicious program!\n");
    fclose(file);

    return 0;
}

