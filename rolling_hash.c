#include "delta_structures.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Rolling hash implementation
// Note: RollingHash is already defined in delta_structures.h

RollingHash* rolling_hash_new(uint32_t window_size) {
  RollingHash* rh = malloc(sizeof(RollingHash));
  if (rh == NULL) {
    printf("Failed to allocate memory for RollingHash\n");
    return NULL;
  }

  rh->a = 0;
  rh->b = 0;
  rh->window_size = window_size;
  rh->window = calloc(window_size, sizeof(uint8_t));  // Initialize to zeros
  if (rh->window == NULL) {
    printf("Failed to allocate memory for window\n");
    free(rh);
    rh = NULL;
    return NULL;
  }

  rh->window_pos = 0;
  rh->bytes_in_window = 0;

  return rh;
}

void rolling_hash_update(RollingHash* rh, uint8_t byte) {
  // Get the old byte that will be overwritten
  uint8_t old_byte = rh->window[rh->window_pos];

  // Store the new byte
  rh->window[rh->window_pos] = byte;

  // Update position (circular buffer)
  rh->window_pos = (rh->window_pos + 1) % rh->window_size;

  // Update hash values
  if (rh->bytes_in_window < rh->window_size) {
    // Window not full yet, just add new byte
    rh->a = (rh->a + byte) % 65521;  // MOD_A = 65521 (largest prime < 2^16)
    rh->b = (rh->b + rh->a) % 65521; // MOD_B = 65521
    rh->bytes_in_window++;
  } else {
    // Window is full, remove old byte and add new byte
    rh->a = (rh->a - old_byte + byte) % 65521;
    rh->b = (rh->b - rh->window_size * old_byte + rh->a) % 65521;
  }
}

uint32_t rolling_hash_get_hash(RollingHash* rh) {
  if (rh == NULL || rh->bytes_in_window == 0) return 0;

  return rh->a ^ rh->b;
}

void rolling_hash_free(RollingHash* rh) {
  if (rh == NULL) return;

  free(rh->window);
  free(rh);
  rh = NULL;
}
