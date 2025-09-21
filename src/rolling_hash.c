#include "delta_structures.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/**
 * @file rolling_hash.c
 * @brief Rolling hash implementation for delta compression pattern matching
 *
 * This module provides a rolling hash implementation optimized for the delta
 * compression algorithm. It uses an Adler-32 inspired approach with bit-shifting
 * for better hash distribution and performance. The rolling hash allows efficient
 * computation of hash values for sliding windows of data without recalculating
 * the entire hash for each position.
 *
 * @author Fiver Development Team
 * @version 1.0
 */

/**
 * @brief Creates a new rolling hash with the specified window size
 *
 * Allocates memory for a new rolling hash structure and initializes all fields.
 * The rolling hash uses a circular buffer to maintain a sliding window of data
 * and computes hash values incrementally as new bytes are added and old bytes
 * are removed from the window.
 *
 * @param window_size Size of the sliding window in bytes. Must be > 0.
 *                    Typical values are 32-64 bytes for good pattern matching.
 *
 * @return Pointer to the newly created RollingHash on success, NULL on failure.
 *         The caller is responsible for freeing the rolling hash with rolling_hash_free().
 *
 * @note Memory allocation failures are logged to stdout with error details.
 *
 * @example
 * ```c
 * RollingHash *rh = rolling_hash_new(32);  // 32-byte sliding window
 * if (rh == NULL) {
 *     // Handle allocation failure
 * }
 * ```
 */
RollingHash * rolling_hash_new(uint32_t window_size)
{
	// Validate input parameters
	if (window_size == 0) {
		printf("Error: window_size must be greater than 0\n");
		return NULL;
	}

	RollingHash *rh = malloc(sizeof(RollingHash));
	if (rh == NULL) {
		printf("Failed to allocate memory for RollingHash: %s\n", strerror(errno));
		return NULL;
	}

	rh->a = 0;
	rh->b = 0;
	rh->window_size = window_size;
	rh->window = calloc(window_size, sizeof(uint8_t)); // Initialize to zeros
	if (rh->window == NULL) {
		printf("Failed to allocate memory for window: %s\n", strerror(errno));
		free(rh);
		return NULL;
	}

	rh->window_pos = 0;
	rh->bytes_in_window = 0;

	return rh;
}

/**
 * @brief Updates the rolling hash with a new byte
 *
 * Adds a new byte to the rolling hash window and removes the oldest byte if the
 * window is full. This function maintains the rolling hash state incrementally,
 * providing O(1) performance for each update operation. The hash values are
 * computed using an Adler-32 inspired algorithm with bit-shifting for better
 * distribution and performance.
 *
 * @param rh Pointer to the rolling hash structure. Must not be NULL.
 * @param byte The new byte to add to the rolling hash window
 *
 * @note This function safely handles NULL pointers and performs no operation
 *       if rh is NULL.
 *
 * @note The rolling hash uses a circular buffer implementation where new bytes
 *       overwrite the oldest bytes when the window is full.
 *
 * @note Hash values are kept in a reasonable range (0-65535) using bit masking
 *       to prevent overflow while maintaining good distribution properties.
 *
 * @example
 * ```c
 * RollingHash *rh = rolling_hash_new(32);
 * // Add bytes one by one
 * rolling_hash_update(rh, 'H');
 * rolling_hash_update(rh, 'e');
 * rolling_hash_update(rh, 'l');
 * rolling_hash_update(rh, 'l');
 * rolling_hash_update(rh, 'o');
 * // After 32 bytes, the window will start rolling
 * ```
 */
void rolling_hash_update(RollingHash *rh, uint8_t byte)
{
	// Validate input parameters
	if (rh == NULL)
		return;

	// Get the old byte that will be overwritten
	uint8_t old_byte = rh->window[rh->window_pos];

	// Store the new byte
	rh->window[rh->window_pos] = byte;

	// Update position (circular buffer)
	rh->window_pos = (rh->window_pos + 1) % rh->window_size;

	// Update hash values using bit operations instead of modulo for speed
	if (rh->bytes_in_window < rh->window_size) {
		// Window not full yet, just add new byte
		rh->a += byte;
		rh->b += rh->a;
		rh->bytes_in_window++;
	} else {
		// Window is full, remove old byte and add new byte
		rh->a = rh->a - old_byte + byte;
		rh->b = rh->b - rh->window_size * old_byte + rh->a;
	}

	// Use bit masking instead of modulo for better performance
	// Keep values in reasonable range to prevent overflow
	if (rh->a > 0xFFFF) rh->a &= 0xFFFF;
	if (rh->b > 0xFFFF) rh->b &= 0xFFFF;
}

/**
 * @brief Gets the current hash value from the rolling hash
 *
 * Computes and returns the current hash value based on the bytes currently
 * in the rolling hash window. The hash is computed by combining the internal
 * 'a' and 'b' values using bit shifting for better distribution.
 *
 * @param rh Pointer to the rolling hash structure. Must not be NULL.
 *
 * @return The current hash value, or 0 if rh is NULL or if no bytes have
 *         been added to the window yet.
 *
 * @note This function returns 0 for an empty window, which is a valid hash
 *       value but may not be useful for pattern matching.
 *
 * @note The hash value is computed as (a << 16) | b, providing good distribution
 *       across the 32-bit hash space.
 *
 * @example
 * ```c
 * RollingHash *rh = rolling_hash_new(32);
 * rolling_hash_update(rh, 'H');
 * rolling_hash_update(rh, 'e');
 * rolling_hash_update(rh, 'l');
 * rolling_hash_update(rh, 'l');
 * rolling_hash_update(rh, 'o');
 *
 * uint32_t hash = rolling_hash_get(rh);
 * printf("Hash value: 0x%08X\n", hash);
 * ```
 */
uint32_t rolling_hash_get(RollingHash *rh)
{
	if (rh == NULL || rh->bytes_in_window == 0)
		return 0;

	// Better hash distribution: combine a and b more effectively
	// Use bit shifting and addition instead of just XOR
	return (rh->a << 16) | rh->b;
}

/**
 * @brief Frees all memory associated with the rolling hash
 *
 * Recursively frees all memory allocated for the rolling hash structure,
 * including the internal window buffer. This function safely handles NULL
 * pointers and ensures no memory leaks occur. After calling this function,
 * the rolling hash pointer should not be used again.
 *
 * @param rh Pointer to the rolling hash to free. Safe to pass NULL.
 *
 * @note This function frees the internal window buffer first, then frees
 *       the rolling hash structure itself.
 *
 * @note After calling this function, the rolling hash pointer becomes invalid
 *       and should not be dereferenced.
 *
 * @example
 * ```c
 * RollingHash *rh = rolling_hash_new(32);
 * // ... use rolling hash ...
 * rolling_hash_free(rh);  // Free all memory
 * // rh is now invalid and should not be used
 * ```
 */
void rolling_hash_free(RollingHash *rh)
{
	if (rh == NULL)
		return;

	free(rh->window);
	free(rh);
}
