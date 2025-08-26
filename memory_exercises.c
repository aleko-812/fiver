#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Exercise 1: Fix the memory leak
void exercise1_fix_memory_leak() {
    printf("Exercise 1: Fix the memory leak\n");

    // TODO: This function has a memory leak. Fix it!
    int* numbers = malloc(10 * sizeof(int));
    for (int i = 0; i < 10; i++) {
        numbers[i] = i * 2;
    }

    printf("Numbers: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", numbers[i]);
    }
    printf("\n");

    // YOUR CODE HERE: Fix the memory leak
    free(numbers);
    numbers = NULL;

    printf("Exercise 1 completed!\n\n");
}

// Exercise 2: Implement a dynamic string builder
struct StringBuilder {
    char* buffer;
    size_t capacity;
    size_t length;
};

struct StringBuilder* string_builder_new(size_t initial_capacity) {
    // TODO: Implement this function
    // 1. Allocate the StringBuilder struct
    // 2. Allocate the initial buffer
    // 3. Initialize the fields
    // 4. Return the StringBuilder

    // YOUR CODE HERE
    struct StringBuilder* sb = malloc(sizeof(struct StringBuilder));
    if (sb == NULL) {
        return NULL;
    }

    // Allocate buffer with initial_capacity bytes
    sb->buffer = calloc(initial_capacity, sizeof(char));
    if (sb->buffer == NULL) {
        free(sb);
        return NULL;
    }

    // Initialize: empty string (just \0 at position 0)
    sb->buffer[0] = '\0';  // This makes it an empty string
    sb->capacity = initial_capacity;
    sb->length = 0;

    return sb;
}

void string_builder_append(struct StringBuilder* sb, const char* str) {
    // TODO: Implement this function
    // 1. Check if we need to resize the buffer
    // 2. Copy the string to the buffer
    // 3. Update the length

    // YOUR CODE HERE
    size_t str_len = strlen(str);
    size_t needed_space = sb->length + str_len + 1; // +1 for \0

    // Check if we need to resize (use doubling strategy)
    if (needed_space > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        if (new_capacity < needed_space) {
            new_capacity = needed_space;
        }

        char* new_buffer = realloc(sb->buffer, new_capacity);
        if (new_buffer == NULL) {
            printf("Failed to reallocate buffer\n");
            return; // Don't free anything, original buffer is still valid
        }

        sb->buffer = new_buffer;
        sb->capacity = new_capacity;
    }

    // Append the string
    strcat(sb->buffer, str);

    // Update length (actual string length, not including \0)
    sb->length += str_len;
}

char* string_builder_to_string(struct StringBuilder* sb) {
    // TODO: Implement this function
    // 1. Allocate a new string with exact size needed
    // 2. Copy the buffer content
    // 3. Return the string (caller is responsible for freeing)

    // YOUR CODE HERE
    char* str = malloc(sb->length + 1);
    strcpy(str, sb->buffer);

    return str;
}

void string_builder_free(struct StringBuilder* sb) {
    // TODO: Implement this function
    // 1. Free the buffer
    // 2. Free the struct

    // YOUR CODE HERE
    free(sb->buffer);
    free(sb);
    sb = NULL;
}

void exercise2_string_builder() {
    printf("Exercise 2: String Builder\n");

    struct StringBuilder* sb = string_builder_new(10);
    if (sb == NULL) {
        printf("Failed to create string builder\n");
        return;
    }

    string_builder_append(sb, "Hello");
    string_builder_append(sb, " ");
    string_builder_append(sb, "World");
    string_builder_append(sb, "!");

    char* result = string_builder_to_string(sb);
    printf("Result: %s\n", result);

    // Cleanup
    free(result);
    string_builder_free(sb);

    printf("Exercise 2 completed!\n\n");
}

// Exercise 3: Implement a simple file buffer
struct FileBuffer {
    unsigned char* data;
    size_t size;
    size_t capacity;
};

struct FileBuffer* file_buffer_new(size_t initial_capacity) {
    // TODO: Implement this function
    // Similar to string_builder_new but for binary data

    // YOUR CODE HERE

    struct FileBuffer* fb = malloc(sizeof(struct FileBuffer));
    if (fb == NULL) {
      printf("Failed to allocate FileBuffer struct\n");
      return NULL;
    }

    fb->data = calloc(initial_capacity, sizeof(unsigned char));
    if (fb->data == NULL) {
      printf("Failed to allocate data buffer\n");
      free(fb);
      return NULL;
    }

    // For binary data, we don't need to initialize with \0
    // The buffer starts empty (size = 0)
    fb->capacity = initial_capacity;
    fb->size = 0;

    return fb;  // Replace with your implementation
}

int file_buffer_append(struct FileBuffer* fb, const unsigned char* data, size_t data_size) {
    // TODO: Implement this function
    // 1. Check if we need to resize
    // 2. Copy the data
    // 3. Update size
    // 4. Return 0 on success, -1 on failure

    // YOUR CODE HERE
    size_t new_size = fb->size + data_size;
    if (new_size > fb->capacity) {
      size_t new_capacity = fb->capacity * 2;
      if(new_capacity < new_size) {
        new_capacity = new_size;
      }

      unsigned char* new_data = realloc(fb->data, new_capacity);
      if (new_data == NULL) {
        printf("Failed to reallocate databuffer\n");

        return -1;
      }

      fb->data = new_data;
      fb->capacity = new_capacity;
    }

    // Copy data to the end of existing data (append, don't overwrite)
    memcpy(fb->data + fb->size, data, data_size);
    fb->size = new_size;

    return 0;  // Replace with your implementation
}

void file_buffer_free(struct FileBuffer* fb) {
    // TODO: Implement this function

    // YOUR CODE HERE
    free(fb->data);
    free(fb);
    fb = NULL;
}

void exercise3_file_buffer() {
    printf("Exercise 3: File Buffer\n");

    struct FileBuffer* fb = file_buffer_new(100);
    if (fb == NULL) {
        printf("Failed to create file buffer\n");
        return;
    }

    // Add some test data
    unsigned char test_data1[] = {0x01, 0x02, 0x03, 0x04};
    unsigned char test_data2[] = {0xFF, 0xFE, 0xFD};

    if (file_buffer_append(fb, test_data1, 4) == 0 &&
        file_buffer_append(fb, test_data2, 3) == 0) {

        printf("File buffer size: %zu\n", fb->size);
        printf("File buffer data: ");
        for (size_t i = 0; i < fb->size; i++) {
            printf("0x%02X ", fb->data[i]);
        }
        printf("\n");
    }

    file_buffer_free(fb);

    printf("Exercise 3 completed!\n\n");
}

int main() {
    printf("Memory Management Exercises\n");
    printf("===========================\n\n");

    exercise1_fix_memory_leak();
    exercise2_string_builder();
    exercise3_file_buffer();

    printf("All exercises completed!\n");
    return 0;
}
