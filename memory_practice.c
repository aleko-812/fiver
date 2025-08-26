#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Example 1: Basic memory allocation
void basic_allocation() {
    printf("=== Basic Memory Allocation ===\n");

    // Stack allocation
    int stack_array[5] = {1, 2, 3, 4, 5};
    printf("Stack array: %d, %d, %d, %d, %d\n",
           stack_array[0], stack_array[1], stack_array[2],
           stack_array[3], stack_array[4]);

    // Heap allocation
    int* heap_array = malloc(5 * sizeof(int));
    if (heap_array == NULL) {
        printf("Memory allocation failed!\n");
        return;
    }

    // Initialize heap array
    for (int i = 0; i < 5; i++) {
        heap_array[i] = i + 1;
    }

    printf("Heap array: %d, %d, %d, %d, %d\n",
           heap_array[0], heap_array[1], heap_array[2],
           heap_array[3], heap_array[4]);

    // IMPORTANT: Free heap memory
    free(heap_array);
    heap_array = NULL;  // Good practice: set to NULL after freeing
}

// Example 2: Common memory management mistakes
void memory_mistakes() {
    printf("\n=== Common Memory Mistakes ===\n");

    // Mistake 1: Memory leak
    printf("Mistake 1: Memory leak\n");
    int* leaky = malloc(100);
    // Oops! Forgot to free(leaky)
    // This memory is now lost until program ends

    // Mistake 2: Double free
    printf("Mistake 2: Double free (commented out to prevent crash)\n");
    int* double_free = malloc(100);
    free(double_free);
    // free(double_free);  // This would crash the program!

    // Mistake 3: Use after free
    printf("Mistake 3: Use after free (commented out to prevent crash)\n");
    int* use_after = malloc(100);
    free(use_after);
    // *use_after = 42;  // This would cause undefined behavior!

    // Mistake 4: Buffer overflow
    printf("Mistake 4: Buffer overflow (commented out to prevent crash)\n");
    char small_buffer[5];
    // strcpy(small_buffer, "This string is too long!");  // Buffer overflow!
}

// Example 3: Proper memory management patterns
void proper_patterns() {
    printf("\n=== Proper Memory Management Patterns ===\n");

    // Pattern 1: Always check malloc return
    int* safe_alloc = malloc(1000);
    if (safe_alloc == NULL) {
        printf("Allocation failed, handling gracefully\n");
        return;
    }

    // Pattern 2: Use the memory
    memset(safe_alloc, 0, 1000);  // Initialize to zero

    // Pattern 3: Free immediately when done
    free(safe_alloc);
    safe_alloc = NULL;

    // Pattern 4: Allocate and initialize in one step
    int* initialized = calloc(10, sizeof(int));  // Allocates and zeros memory
    if (initialized != NULL) {
        printf("calloc initialized array with zeros\n");
        free(initialized);
    }
}

// Example 4: Dynamic arrays
void dynamic_arrays() {
    printf("\n=== Dynamic Arrays ===\n");

    // Allocate dynamic array
    int size = 10;
    int* array = malloc(size * sizeof(int));
    if (array == NULL) {
        printf("Failed to allocate array\n");
        return;
    }

    // Fill array
    for (int i = 0; i < size; i++) {
        array[i] = i * i;
    }

    // Print array
    printf("Array contents: ");
    for (int i = 0; i < size; i++) {
        printf("%d ", array[i]);
    }
    printf("\n");

    // Resize array (realloc)
    size = 15;
    int* new_array = realloc(array, size * sizeof(int));
    if (new_array == NULL) {
        printf("Failed to resize array\n");
        free(array);  // Free original if realloc fails
        return;
    }
    array = new_array;

    // Add more values
    for (int i = 10; i < size; i++) {
        array[i] = i * i;
    }

    printf("Resized array: ");
    for (int i = 0; i < size; i++) {
        printf("%d ", array[i]);
    }
    printf("\n");

    free(array);
}

// Example 5: Struct allocation
struct Person {
    char* name;
    int age;
    float height;
};

void struct_allocation() {
    printf("\n=== Struct Allocation ===\n");

    // Allocate struct on heap
    struct Person* person = malloc(sizeof(struct Person));
    if (person == NULL) {
        printf("Failed to allocate person struct\n");
        return;
    }

    // Allocate string for name
    person->name = malloc(50);
    if (person->name == NULL) {
        printf("Failed to allocate name\n");
        free(person);
        return;
    }

    // Initialize struct
    strcpy(person->name, "John Doe");
    person->age = 30;
    person->height = 1.75;

    printf("Person: %s, Age: %d, Height: %.2f\n",
           person->name, person->age, person->height);

    // Free in reverse order of allocation
    free(person->name);
    free(person);
}

int main() {
    printf("C Memory Management Practice\n");
    printf("============================\n\n");

    basic_allocation();
    memory_mistakes();
    proper_patterns();
    dynamic_arrays();
    struct_allocation();

    printf("\n=== Practice Complete ===\n");
    return 0;
}


