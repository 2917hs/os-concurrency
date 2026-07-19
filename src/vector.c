#include "vector.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>

void vector_init(vector_t *v, int id, int length, unsigned int fill)
{
    v->values = xmalloc((size_t)length * sizeof(*v->values));
    v->length = length;
    v->id     = id;
    for (int i = 0; i < length; i++)
        v->values[i] = fill;
    mutex_init(&v->lock);
}

void vector_destroy(vector_t *v)
{
    mutex_destroy(&v->lock);
    free(v->values);
    v->values = NULL;
    v->length = 0;
}

void vector_add_locked_region(vector_t *dst, const vector_t *src)
{
    /* Callers guarantee mutual exclusion; this is the shared critical-section
     * body so every strategy performs identical work. */
    for (int i = 0; i < dst->length; i++)
        dst->values[i] += src->values[i];
}

void vector_print(const vector_t *v, FILE *out)
{
    fprintf(out, "%d: [", v->id);
    for (int i = 0; i < v->length; i++)
        fprintf(out, "%s%u", i ? ", " : "", v->values[i]);
    fprintf(out, "]\n");
}
