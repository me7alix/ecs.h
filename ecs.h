#ifndef ECS_H_
#define ECS_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ECS_NULL_INDEX UINT32_MAX

typedef uint32_t ECS_Entity;
typedef uint32_t ECS_ComponentType;

#define ECS_DA(T)        \
	struct {             \
		T *items;        \
		size_t count;    \
		size_t capacity; \
	}

typedef struct {
    void *data;
    ECS_Entity *index_to_entity;

    size_t count;
    size_t capacity;
	size_t element_size;
	ECS_ComponentType type_id;
} ECS_ComponentPool;

typedef struct {
	uint32_t *sparse_matrix;
	size_t max_entities;

	ECS_DA(ECS_Entity) pending_entity_removals;
	ECS_DA(ECS_ComponentPool) component_pools;
	ECS_DA(ECS_Entity) available_ids;
	ECS_Entity id_counter;
} ECS;

ECS ecs_new(size_t capacity);
ECS_ComponentType ecs_register_component(ECS *e, size_t element_size, size_t capacity);

ECS_Entity ecs_add_entity(ECS *e);
void ecs_remove_entity(ECS *e, ECS_Entity entity);

void ecs_add_component(ECS *e, ECS_ComponentType type_id, ECS_Entity entity, void *component_data);
void *ecs_get_component(ECS *e, ECS_ComponentType type_id, ECS_Entity entity);
void ecs_remove_component(ECS *e, ECS_ComponentType type_id, ECS_Entity entity);

static inline bool ecs_has_component(ECS *e, ECS_ComponentType type_id, ECS_Entity entity) {
	return e->sparse_matrix[entity * e->component_pools.count + type_id] != ECS_NULL_INDEX;
}

#define ECS_GET_COMPONENT(ecs, type_id, Type, entity) \
	((Type*)ecs_get_component(ecs, type_id, entity))

#define ECS_EACH(ecs, component_type, Type, item_var, entity_var)                     \
	ECS_ComponentPool *_pool = (ecs)->component_pools.items + (component_type);       \
	Type *item_var = (Type*)_pool->data + _pool->count - 1;                           \
	ECS_Entity *entity_var = _pool->index_to_entity + _pool->count - 1;               \
	for (int _iter = _pool->count - 1; _iter >= 0; _iter--, item_var--, entity_var--) \

#define ecs_da_append(da, item)                                \
	do {                                                       \
		if ((da)->capacity == 0) (da)->capacity = 256;         \
		if ((da)->count >= (da)->capacity - 1 || !(da)->items) \
			(da)->items = realloc((da)->items, sizeof(*(da)->items) * ((da)->capacity *= 2)); \
		(da)->items[(da)->count++] = (item);                   \
	} while (0)

#define ecs_da_free(da)         \
	do {                        \
		if ((da)->items) {      \
			free((da)->items);  \
			(da)->items = NULL; \
			(da)->count = 0;    \
			(da)->capacity = 0; \
		}                       \
	} while (0)

#endif // ECS_H_

#ifdef ECS_IMPLEMENTATION

uint32_t ecs_register_component(ECS *e, size_t element_size, size_t capacity) {
	ECS_ComponentPool cp;
	if (element_size == 0) cp.data = NULL;
	else cp.data = malloc(capacity * element_size);
	cp.index_to_entity = malloc(capacity * sizeof(uint32_t));
	cp.element_size = element_size;
	cp.capacity = capacity;
	cp.type_id = e->component_pools.count;
	cp.count = 0;
	ecs_da_append(&e->component_pools, cp);
	return cp.type_id;
}

ECS ecs_new(size_t capacity) {
	return (ECS){
		.sparse_matrix = malloc(capacity * sizeof(uint32_t)),
		.max_entities = capacity,
		.id_counter = 0,
	};
}

ECS_Entity ecs_add_entity(ECS *e) {
	if (e->available_ids.count > 0) {
		return e->available_ids.items[--e->available_ids.count];
	}

	ECS_Entity entity = e->id_counter++;
	if (entity >= e->max_entities) {
		size_t old_cap = e->max_entities;
		e->max_entities *= 2;
		size_t type_count = e->component_pools.count;
		e->sparse_matrix = realloc(e->sparse_matrix, e->max_entities * type_count * sizeof(uint32_t));
		size_t sz = (e->max_entities - old_cap) * type_count * sizeof(uint32_t);
		memset(e->sparse_matrix + (old_cap * type_count), 0xFF, sz);
	}

	return entity;
}

void ecs_add_component(ECS *e, ECS_ComponentType type_id, ECS_Entity entity, void *component_data) {
	ECS_ComponentPool *p = &e->component_pools.items[type_id];
	if (p->count >= p->capacity) {
		p->capacity *= 2;
		if (p->element_size > 0) p->data = realloc(p->data, p->capacity * p->element_size);
		p->index_to_entity = realloc(p->index_to_entity, p->capacity * sizeof(uint32_t));
	}

	size_t dense_idx = p->count++;
	p->index_to_entity[dense_idx] = entity;
	e->sparse_matrix[entity * e->component_pools.count + type_id] = (uint32_t)dense_idx;

	if (component_data && p->element_size > 0) {
		memcpy((char*)p->data + (dense_idx * p->element_size), component_data, p->element_size);
	}
}

void *ecs_get_component(ECS *e, ECS_ComponentType type_id, ECS_Entity entity) {
	uint32_t dense_idx = e->sparse_matrix[entity * e->component_pools.count + type_id];
	if (dense_idx == ECS_NULL_INDEX) return NULL;

	ECS_ComponentPool *p = &e->component_pools.items[type_id];
	return (char*)p->data + (dense_idx * p->element_size);
}

void ecs_remove_component(ECS *e, ECS_ComponentType type_id, ECS_Entity entity) {
	size_t num_types = e->component_pools.count;
	ECS_ComponentPool *p = &e->component_pools.items[type_id];
	size_t dense_idx = e->sparse_matrix[entity * num_types + type_id];

	if (dense_idx == ECS_NULL_INDEX) return;

	if (p->count > 1 && dense_idx < p->count - 1) {
		uint32_t last_entity = p->index_to_entity[p->count - 1];

		if (p->element_size > 0) {
			memcpy(
				(char*)p->data + dense_idx * p->element_size,
				(char*)p->data + (p->count - 1) * p->element_size,
				p->element_size
			);
		}

		p->index_to_entity[dense_idx] = last_entity;
		e->sparse_matrix[last_entity * num_types + type_id] = dense_idx;
	}

	e->sparse_matrix[entity * num_types + type_id] = ECS_NULL_INDEX;
	p->count--;
}

void ecs_remove_entity(ECS *e, ECS_Entity entity) {
	ecs_da_append(&e->pending_entity_removals, entity);
}

void ecs_flush_deletions(ECS *e) {
	for (size_t i = 0; i < e->pending_entity_removals.count; i++) {
		ECS_Entity entity = e->pending_entity_removals.items[i];

		for (size_t j = 0; j < e->component_pools.count; j++) {
			ecs_remove_component(e, (uint32_t)j, entity);
		}

		ecs_da_append(&e->available_ids, entity);
	}

	e->pending_entity_removals.count = 0;
}

void ecs_free(ECS *e) {
	free(e->sparse_matrix);

	ecs_da_free(&e->pending_entity_removals);
	ecs_da_free(&e->available_ids);

	for (size_t i = 0; i < e->component_pools.count; i++) {
		free(e->component_pools.items[i].data);
		free(e->component_pools.items[i].index_to_entity);
	}

	ecs_da_free(&e->component_pools);
}

#endif // ECS_IMPLEMENTATION
