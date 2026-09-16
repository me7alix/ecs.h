#include <stdio.h>
#include <assert.h>
#include <time.h>

#define ECS_IMPLEMENTATION
#include "ecs.h"

typedef struct {
	int health;
} Health;

struct {
	ECS ecs;
	ECS_ComponentType health;
} world;

void health_system_start() {
	ECS_EACH (&world.ecs, world.health, Health, h, entity) {
		h->health = rand() % 200;

		printf("str %u -> %d\n", *entity, h->health);
	}
}

void health_system_update() {
	ECS_EACH (&world.ecs, world.health, Health, h, entity) {
		h->health -= 25;

		printf("dmg %u -> %d\n", *entity, h->health);

		if (h->health <= 0) {
			ecs_remove_entity(&world.ecs, *entity);
		}
	}
}

int main(void) {
	srand(time(NULL));

	world.ecs = ecs_new(1024);
	world.health = ecs_register_component(&world.ecs, sizeof(Health), 1024);

	for (int i = 0; i < 6; i++) {
		ecs_add_component(&world.ecs, world.health, ecs_add_entity(&world.ecs), NULL);
	}

	health_system_start();

	for (int i = 0; i < 10; i++) {
		printf("-- ITER %d --\n", i);

		health_system_update();

		ecs_flush_deletions(&world.ecs);
	}

	ecs_free(&world.ecs);
	return 0;
}
