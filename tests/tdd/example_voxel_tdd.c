#include "tdd_framework.h"

typedef struct {
    int x, y, z;
    unsigned char r, g, b, a;
} voxel_t;

typedef struct {
    voxel_t* voxels;
    int count;
    int capacity;
} voxel_array_t;

voxel_array_t* voxel_array_create(int initial_capacity) {
    voxel_array_t* array = malloc(sizeof(voxel_array_t));
    if (!array) return NULL;
    
    array->voxels = malloc(sizeof(voxel_t) * initial_capacity);
    if (!array->voxels) {
        free(array);
        return NULL;
    }
    
    array->count = 0;
    array->capacity = initial_capacity;
    return array;
}

void voxel_array_destroy(voxel_array_t* array) {
    if (array) {
        free(array->voxels);
        free(array);
    }
}

int voxel_array_add(voxel_array_t* array, voxel_t voxel) {
    if (!array) return -1;
    
    if (array->count >= array->capacity) {
        int new_capacity = array->capacity * 2;
        voxel_t* new_voxels = realloc(array->voxels, sizeof(voxel_t) * new_capacity);
        if (!new_voxels) return -1;
        
        array->voxels = new_voxels;
        array->capacity = new_capacity;
    }
    
    array->voxels[array->count++] = voxel;
    return 0;
}

voxel_t* voxel_array_find(voxel_array_t* array, int x, int y, int z) {
    if (!array) return NULL;
    
    for (int i = 0; i < array->count; i++) {
        if (array->voxels[i].x == x && 
            array->voxels[i].y == y && 
            array->voxels[i].z == z) {
            return &array->voxels[i];
        }
    }
    return NULL;
}

int test_voxel_array_create_destroy() {
    voxel_array_t* array = voxel_array_create(10);
    TEST_ASSERT(array != NULL, "Array should be created");
    TEST_ASSERT_EQ(0, array->count);
    TEST_ASSERT_EQ(10, array->capacity);
    
    voxel_array_destroy(array);
    return 1;
}

int test_voxel_array_add_single() {
    voxel_array_t* array = voxel_array_create(10);
    voxel_t voxel = {.x = 1, .y = 2, .z = 3, .r = 255, .g = 0, .b = 0, .a = 255};
    
    TEST_ASSERT_EQ(0, voxel_array_add(array, voxel));
    TEST_ASSERT_EQ(1, array->count);
    
    voxel_array_destroy(array);
    return 1;
}

int test_voxel_array_add_multiple() {
    voxel_array_t* array = voxel_array_create(2);
    
    for (int i = 0; i < 5; i++) {
        voxel_t voxel = {.x = i, .y = i, .z = i, .r = 255, .g = 0, .b = 0, .a = 255};
        TEST_ASSERT_EQ(0, voxel_array_add(array, voxel));
    }
    
    TEST_ASSERT_EQ(5, array->count);
    TEST_ASSERT(array->capacity >= 5, "Capacity should have grown");
    
    voxel_array_destroy(array);
    return 1;
}

int test_voxel_array_find_existing() {
    voxel_array_t* array = voxel_array_create(10);
    voxel_t voxel = {.x = 5, .y = 10, .z = 15, .r = 255, .g = 128, .b = 64, .a = 255};
    
    voxel_array_add(array, voxel);
    
    voxel_t* found = voxel_array_find(array, 5, 10, 15);
    TEST_ASSERT(found != NULL, "Should find existing voxel");
    TEST_ASSERT_EQ(255, found->r);
    TEST_ASSERT_EQ(128, found->g);
    TEST_ASSERT_EQ(64, found->b);
    
    voxel_array_destroy(array);
    return 1;
}

int test_voxel_array_find_non_existing() {
    voxel_array_t* array = voxel_array_create(10);
    voxel_t voxel = {.x = 5, .y = 10, .z = 15, .r = 255, .g = 128, .b = 64, .a = 255};
    
    voxel_array_add(array, voxel);
    
    voxel_t* found = voxel_array_find(array, 1, 1, 1);
    TEST_ASSERT(found == NULL, "Should not find non-existing voxel");
    
    voxel_array_destroy(array);
    return 1;
}

int test_null_safety() {
    TEST_ASSERT_EQ(-1, voxel_array_add(NULL, (voxel_t){0}));
    TEST_ASSERT(voxel_array_find(NULL, 0, 0, 0) == NULL, "Find should handle NULL array");
    
    voxel_array_destroy(NULL);
    return 1;
}

int main() {
    TEST_SUITE_BEGIN();
    
    RUN_TEST(test_voxel_array_create_destroy);
    RUN_TEST(test_voxel_array_add_single);
    RUN_TEST(test_voxel_array_add_multiple);
    RUN_TEST(test_voxel_array_find_existing);
    RUN_TEST(test_voxel_array_find_non_existing);
    RUN_TEST(test_null_safety);
    
    TEST_SUITE_END();
    
    return g_test_stats.failed_tests > 0 ? 1 : 0;
}