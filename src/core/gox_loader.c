/* Goxel 3D voxels editor - Simplified GOX Loader
 *
 * copyright (c) 2015-2025 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel_core.h"
#include "../goxel.h"
#include "../utils/img.h"
#include <errno.h>
#include <string.h>
#include <assert.h>

#define VERSION 12

typedef struct {
    uint64_t uid;
    uint8_t *v;
    UT_hash_handle hh;
} block_hash_t;

static block_hash_t *hash_find_at(block_hash_t *hash, int index)
{
    int i;
    for (i = 0; i < index; i++) {
        if (!hash) return NULL;
        hash = hash->hh.next;
    }
    return hash;
}

typedef struct {
    char     type[4];
    uint32_t length;            // Length of the next byte data.
    uint32_t data_length;       // Useful data length.
    uint8_t  *buffer;           // Buffer to hold all the data.
    uint8_t  *data;             // Pointer to the data (skipping the dict).
} chunk_t;

static uint32_t read_int32(FILE *f)
{
    uint32_t v;
    size_t r = fread(&v, 4, 1, f);
    (void)r;
    return v;
}

static bool chunk_read_start(chunk_t *c, FILE *in)
{
    size_t r;
    c->buffer = NULL;
    c->data = NULL;
    r = fread(c->type, 4, 1, in);
    if (r != 1) {
        LOG_D("DEBUG: chunk_read_start returning false, fread returned %zu", r);
        return false;
    }
    c->length = read_int32(in);
    c->data_length = c->length;
    LOG_D("DEBUG: Read chunk type '%.4s' length %d", c->type, c->length);
    return true;
}

static void chunk_read(chunk_t *c, FILE *in, char *buff, int size, int pos)
{
    int r;
    if (buff) {
        r = fread(buff, size, 1, in);
        (void)r;
    } else {
        fseek(in, size, SEEK_CUR);
    }
}

static void chunk_read_finish(chunk_t *c, FILE *in, long data_start_pos)
{
    // First, ensure we're at the end of the chunk data
    long current_pos = ftell(in);
    long expected_end = data_start_pos + c->length;
    
    if (current_pos < expected_end) {
        long skip_bytes = expected_end - current_pos;
        LOG_D("DEBUG: Skipping %ld remaining bytes before CRC", skip_bytes);
        fseek(in, skip_bytes, SEEK_CUR);
    }
    
    // Read the 4-byte CRC at the end of the chunk
    uint32_t crc;
    long pos_before = ftell(in);
    size_t r = fread(&crc, 4, 1, in);
    long pos_after = ftell(in);
    LOG_D("DEBUG: chunk_read_finish - read CRC from %ld to %ld (value: 0x%08x)", pos_before, pos_after, crc);
    (void)r; // Ignore CRC for now
    
    free(c->buffer);
    c->buffer = NULL;
}

static uint32_t chunk_read_int32(chunk_t *c, FILE *in, int pos)
{
    uint32_t v;
    chunk_read(c, in, (char*)&v, 4, pos);
    return v;
}

static char *chunk_read_string(chunk_t *c, FILE *in, int pos)
{
    uint32_t size;
    char *ret;
    chunk_read(c, in, (char*)&size, 4, pos);
    ret = calloc(1, size + 1);
    chunk_read(c, in, ret, size, pos);
    return ret;
}

static bool chunk_read_dict_value(chunk_t *c, FILE *in,
                                  char *key, char *value, int *value_size,
                                  int pos)
{
    char *k, *v;
    int size = sizeof(uint32_t);
    long pos_before = ftell(in);
    if (c->data - c->buffer >= c->length - c->data_length - 4) return false;
    k = chunk_read_string(c, in, pos);
    size += sizeof(uint32_t) + strlen(k);
    if (!c->data) {
        c->data = c->buffer + 2 * sizeof(uint32_t) + strlen(k);
    }
    
    if (*k == '\0') {
        LOG_D("DEBUG: End of dictionary marker found at position %ld", pos_before);
        free(k);
        return false;
    }
    
    if (value) {
        chunk_read(c, in, (char*)value_size, 4, pos);
        size += sizeof(uint32_t);
        chunk_read(c, in, value, *value_size, pos);
        size += *value_size;
    } else {
        *value_size = chunk_read_int32(c, in, pos);
        size += sizeof(uint32_t);
        v = calloc(1, *value_size + 1);
        chunk_read(c, in, v, *value_size, pos);
        size += *value_size;
        free(v);
    }
    
    if (key) sprintf(key, "%s", k);
    free(k);
    c->data_length -= size;
    return true;
}

#define DICT_CPY(n, v) ({ \
    char *_n = (n); \
    typeof(v) *_v = &(v); \
    bool _r = false; \
    if (strcmp(dict_key, _n) == 0) { \
        if (dict_value_size == sizeof(v)) { \
            memcpy(_v, dict_value, sizeof(v)); \
            _r = true; \
        } else { \
            LOG_W("Cannot parse dict value %s", _n); \
        } \
    } \
    _r; })

int load_gox_file_to_image(const char *path, image_t *image)
{
    layer_t *layer;
    block_hash_t *blocks_table = NULL, *data, *data_tmp;
    FILE *in;
    char magic[4] = {};
    uint8_t *voxel_data;
    int nb_blocks;
    int w, h, bpp;
    uint8_t *png;
    chunk_t c;
    int i, index, version, x, y, z;
    int dict_value_size;
    char dict_key[256];
    char dict_value[256];
    uint64_t uid = 1;
    int aabb[2][3];
    
    LOG_D("Opening GOX file: %s", path);
    
    in = fopen(path, "rb");
    if (!in) {
        LOG_E("Cannot open file: %s", path);
        return -1;
    }
    
    if (fread(magic, 4, 1, in) != 1) goto error;
    if (strncmp(magic, "GOX ", 4) != 0) {
        LOG_E("Not a GOX file");
        goto error;
    }
    
    version = read_int32(in);
    if (version > VERSION) {
        LOG_W("Cannot open gox file version %d", version);
        goto error;
    }
    
    LOG_D("GOX file version: %d", version);
    
    // Read chunks
    while (chunk_read_start(&c, in)) {
        long pos_after_header = ftell(in);
        long chunk_start_pos = pos_after_header - 8; // 4 bytes type + 4 bytes length
        LOG_I("DEBUG: Chunk starts at %ld, data starts at %ld", chunk_start_pos, pos_after_header);
        LOG_I("DEBUG: File position after reading chunk header: %ld", pos_after_header);
        LOG_D("Reading chunk: %.4s (length: %d)", c.type, c.length);
        
        // Sanity check chunk length
        if (c.length > 10000000) { // 10MB max chunk size
            LOG_E("ERROR: Invalid chunk length %d for chunk %.4s", c.length, c.type);
            // Print chunk type bytes
            LOG_E("Chunk type bytes: %02x %02x %02x %02x", 
                  (unsigned char)c.type[0], (unsigned char)c.type[1], 
                  (unsigned char)c.type[2], (unsigned char)c.type[3]);
            // Try to recover by seeking back and attempting to find next valid chunk
            fseek(in, pos_after_header - 8, SEEK_SET);
            // Skip 4 bytes at a time looking for valid chunk
            int attempts = 0;
            while (attempts < 100) {
                fseek(in, 4, SEEK_CUR);
                long test_pos = ftell(in);
                char test_type[4];
                if (fread(test_type, 4, 1, in) == 1) {
                    if (strncmp(test_type, "BL16", 4) == 0 || 
                        strncmp(test_type, "LAYR", 4) == 0 ||
                        strncmp(test_type, "CAMR", 4) == 0 ||
                        strncmp(test_type, "MATE", 4) == 0 ||
                        strncmp(test_type, "LIGH", 4) == 0) {
                        LOG_I("Found valid chunk %.4s at position %ld, attempting recovery", test_type, test_pos);
                        fseek(in, test_pos, SEEK_SET);
                        break;
                    }
                }
                fseek(in, test_pos, SEEK_SET);
                attempts++;
            }
            if (attempts >= 100) break;
        }
        
        long pos_before_chunk = ftell(in);
        LOG_D("DEBUG: About to process chunk %.4s at position %ld", c.type, pos_before_chunk);
        
        if (strncmp(c.type, "BL16", 4) == 0) {
            // Block data
            LOG_I("DEBUG: Reading BL16 chunk of size %d", c.length);
            png = calloc(1, c.length);
            chunk_read(&c, in, (char*)png, c.length, __LINE__);
            bpp = 4;
            voxel_data = img_read_from_mem((void*)png, c.length, &w, &h, &bpp);
            if (w == 64 && h == 64 && bpp == 4) {
                data = calloc(1, sizeof(*data));
                data->v = calloc(1, 64 * 64 * 4);
                memcpy(data->v, voxel_data, 64 * 64 * 4);
                data->uid = ++uid;
                HASH_ADD(hh, blocks_table, uid, sizeof(data->uid), data);
                LOG_I("DEBUG: Added block with UID %llu to blocks_table", (unsigned long long)data->uid);
            } else {
                LOG_E("DEBUG: Invalid BL16 dimensions: %dx%d, bpp=%d", w, h, bpp);
            }
            free(voxel_data);
            free(png);
            
        } else if (strncmp(c.type, "LAYR", 4) == 0) {
            // Layer data
            layer = image_add_layer(image, NULL);
            nb_blocks = chunk_read_int32(&c, in, __LINE__);
            LOG_D("Layer with %d blocks", nb_blocks);
            LOG_I("DEBUG: Loading LAYR chunk with %d blocks", nb_blocks);
            
            for (i = 0; i < nb_blocks; i++) {
                index = chunk_read_int32(&c, in, __LINE__);
                x = chunk_read_int32(&c, in, __LINE__);
                y = chunk_read_int32(&c, in, __LINE__);
                z = chunk_read_int32(&c, in, __LINE__);
                if (version == 1) { // Previous version blocks pos.
                    x -= 8; y -= 8; z -= 8;
                }
                chunk_read_int32(&c, in, __LINE__); // Skip unused field
                
                data = hash_find_at(blocks_table, index);
                if (data) {
                    LOG_I("DEBUG: Blitting block %d at position (%d, %d, %d)", index, x, y, z);
                    volume_blit(layer->volume, data->v, x, y, z, 16, 16, 16, NULL);
                } else {
                    LOG_E("DEBUG: Block %d not found in blocks_table!", index);
                }
            }
            
            // Read layer properties
            int material_idx = 0;
            while (chunk_read_dict_value(&c, in, dict_key, dict_value,
                                         &dict_value_size, __LINE__)) {
                if (strcmp(dict_key, "name") == 0) {
                    snprintf(layer->name, sizeof(layer->name), "%s", dict_value);
                }
                DICT_CPY("visible", layer->visible);
                DICT_CPY("color", layer->color);
                DICT_CPY("box", layer->box);
                DICT_CPY("mode", layer->mode);
                DICT_CPY("material", material_idx);
            }
            
            // Set the layer's material from the material index
            if (material_idx >= 0) {
                material_t *mat = image->materials;
                int i = 0;
                while (mat && i < material_idx) {
                    mat = mat->next;
                    i++;
                }
                if (mat) {
                    layer->material = mat;
                    LOG_I("DEBUG: Layer '%s' assigned material '%s' (index %d)", 
                          layer->name, mat->name, material_idx);
                } else {
                    LOG_W("DEBUG: Material index %d not found for layer '%s'", 
                          material_idx, layer->name);
                }
            }
            
            // The empty dictionary terminator consumed 4 bytes, rewind
            fseek(in, -4, SEEK_CUR);
            
        } else if (strncmp(c.type, "IMG ", 4) == 0) {
            // Image properties
            LOG_I("DEBUG: Reading IMG dictionary");
            while (chunk_read_dict_value(&c, in, dict_key, dict_value,
                                         &dict_value_size, __LINE__)) {
                LOG_I("DEBUG: Read IMG dict key: %s", dict_key);
                DICT_CPY("box", image->box);
            }
            LOG_I("DEBUG: Finished reading IMG dictionary");
            
            // The empty dictionary terminator consumed 4 bytes, rewind
            fseek(in, -4, SEEK_CUR);
            LOG_D("DEBUG: Rewound 4 bytes after dictionary end");
            
        } else if (strncmp(c.type, "PREV", 4) == 0) {
            // Skip preview in headless mode
            LOG_I("DEBUG: Skipping PREV chunk of size %d", c.length);
            chunk_read(&c, in, NULL, c.length, __LINE__);
            LOG_I("DEBUG: PREV chunk skipped, file position: %ld", ftell(in));
            
        } else if (strncmp(c.type, "MATE", 4) == 0) {
            // Read material chunk
            material_t *mat = image_add_material(image, NULL);
            LOG_I("DEBUG: Reading MATE chunk");
            while (chunk_read_dict_value(&c, in, dict_key, dict_value,
                                         &dict_value_size, __LINE__)) {
                if (strcmp(dict_key, "name") == 0) {
                    snprintf(mat->name, sizeof(mat->name), "%s", dict_value);
                }
                DICT_CPY("color", mat->base_color);
                DICT_CPY("metallic", mat->metallic);
                DICT_CPY("roughness", mat->roughness);
                DICT_CPY("emission", mat->emission);
            }
            // The empty dictionary terminator consumed 4 bytes, rewind
            fseek(in, -4, SEEK_CUR);
            LOG_I("DEBUG: Material '%s' loaded", mat->name);
            
        } else {
            // Skip unknown chunks
            chunk_read(&c, in, NULL, c.length, __LINE__);
        }
        
        chunk_read_finish(&c, in, pos_after_header);
    }
    
    // Free the block hash table
    HASH_ITER(hh, blocks_table, data, data_tmp) {
        HASH_DEL(blocks_table, data);
        free(data->v);
        free(data);
    }
    
    // Add a default camera if there is none
    if (!image->cameras) {
        image_add_camera(image, NULL);
        if (!box_is_null(image->box)) {
            camera_fit_box(image->active_camera, image->box);
        }
    }
    
    // Set default image box if we didn't have one
    if (box_is_null(image->box)) {
        // Create a simple default box
        aabb[0][0] = -16;
        aabb[0][1] = -16;
        aabb[0][2] = 0;
        aabb[1][0] = 16;
        aabb[1][1] = 16;
        aabb[1][2] = 32;
        bbox_from_aabb(image->box, aabb);
    }
    
    fclose(in);
    LOG_I("Successfully loaded GOX file");
    return 0;
    
error:
    fclose(in);
    LOG_E("Failed to load GOX file");
    return -1;
}