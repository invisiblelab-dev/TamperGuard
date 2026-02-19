#ifndef __enforcement_H__
#define __enforcement_H__

#include "../../shared/types/layer_context.h"
#include "demultiplexer.h"
#include <pthread.h>

/**
 * @brief Wait for all threads to complete before any cleanup
 *
 * @param threads       -> array of threads
 * @param active_threads -> number of active threads
 * @param nlayers       -> number of layers
 * @param state         -> demultiplexer state containing options
 */
void wait_for_all_threads(pthread_t *threads, int active_threads, int nlayers,
                          DemultiplexerState *state);

/**
 * @brief Check if all enforced layers succeeded in their ssize_t operations
 *
 * @param results       -> array of ssize_t operation results
 * @param nlayers       -> number of layers
 * @param state         -> demultiplexer state containing options
 * @return ssize_t      -> the result of the first enforced layer that
 * succeeded, -1 if one failed
 */
ssize_t get_enforced_layers_ssize_result(ssize_t *results, int nlayers,
                                         DemultiplexerState *state);

/**
 * @brief Check if all enforced layers succeeded in their int operations
 *
 * @param results       -> array of int operation results
 * @param nlayers       -> number of layers
 * @param state         -> demultiplexer state containing options
 * @return int          -> the result of the first enforced layer that
 * succeeded, -1 if one failed
 */
int get_enforced_layers_int_result(int *results, int nlayers,
                                   DemultiplexerState *state);

#endif // __enforcement_H__
