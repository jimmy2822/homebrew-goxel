/* Goxel 3D voxels editor
 *
 * copyright (c) 2019 Guillaume Chereau <guillaume@noctua-software.com>
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

#include "goxel.h"

#include "file_format.h"
#include "utils/vec.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#include "../ext_src/cgltf/cgltf_write.h"
#pragma GCC diagnostic pop

typedef struct {
    cgltf_data *data;
    palette_t palette;
    cgltf_material *default_mat;
} gltf_t;

typedef struct {
    float   pos[3];
    float   normal[3];

    // XXX: for vertex color we are wasting space here.
    union {
        float color[4];
        float texcoord[2];
    };
} gltf_vertex_t;

typedef struct {
    bool vertex_color;
    float simplify;
} export_options_t;

static export_options_t g_export_options = {};


// Return the next power of 2 larger or equal to x.
static int next_pow2(int x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

static size_t base64_encode(const uint8_t *data, size_t len, char *buf)
{
    const char table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                          'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                          'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                          'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                          'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                          'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                          'w', 'x', 'y', 'z', '0', '1', '2', '3',
                          '4', '5', '6', '7', '8', '9', '+', '/'};
    const int mod_table[] = {0, 2, 1};
    uint32_t a, b, c, triple;
    int i, j;
    size_t out_len = 4 * ((len + 2) / 3);
    if (!buf) return out_len;
    for (i = 0, j = 0; i < len;) {
        a = i < len ? data[i++] : 0;
        b = i < len ? data[i++] : 0;
        c = i < len ? data[i++] : 0;
        triple = (a << 0x10) + (b << 0x08) + c;

        buf[j++] = table[(triple >> 3 * 6) & 0x3F];
        buf[j++] = table[(triple >> 2 * 6) & 0x3F];
        buf[j++] = table[(triple >> 1 * 6) & 0x3F];
        buf[j++] = table[(triple >> 0 * 6) & 0x3F];
    }
    for (i = 0; i < mod_table[len % 3]; i++)
        buf[out_len - 1 - i] = '=';

    return out_len;
}

static char *data_new(const void *data, uint32_t len, const char *mime)
{
    char *string;
    if (!mime) mime = "application/octet-stream";
    string = calloc(strlen("data:") + strlen(mime) + strlen(";base64,") +
                    base64_encode(data, len, NULL) + 1, 1);
    sprintf(string, "data:%s;base64,", mime);
    base64_encode(data, len, string + strlen(string));
    return string;
}

#define DL_SIZE(head) ({ \
    int size = 0; \
    typeof(*(head)) *tmp; \
    DL_COUNT(head, tmp, size); \
    size; \
})

#define ALLOC(array, size_)                                                   \
    ({                                                                        \
        int size = size_;                                                     \
        if (size > 0) {                                                       \
            (array) = calloc(size, sizeof(*(array)));                         \
        }                                                                     \
    })

static void gltf_init(gltf_t *g, const export_options_t *options,
                      const image_t *img)
{
    const layer_t *layer;
    volume_iterator_t iter;
    int bpos[3], nb_blocks = 0;

    g->data = calloc(1, sizeof(*g->data));
    g->data->memory.free_func = &cgltf_default_free;
    g->data->asset.version = strdup("2.0");
    g->data->asset.generator = strdup("goxel");

    // Count the total number of blocks.
    DL_FOREACH(img->layers, layer) {
        iter = volume_get_iterator(layer->volume,
                VOLUME_ITER_TILES | VOLUME_ITER_INCLUDES_NEIGHBORS);
        while (volume_iter(&iter, bpos)) {
            nb_blocks++;
        }
    }

    // Initialize all the gltf base object arrays.
    ALLOC(g->data->materials, DL_SIZE(img->materials) + 1);
    ALLOC(g->data->scenes, 1);
    ALLOC(g->data->nodes, 1 + nb_blocks + DL_SIZE(img->layers));
    ALLOC(g->data->meshes, nb_blocks);
    ALLOC(g->data->accessors, nb_blocks * 4);
    ALLOC(g->data->buffers, nb_blocks * 2 + 1);
    ALLOC(g->data->buffer_views, nb_blocks * 2 + 1);
    ALLOC(g->data->images, 1);
    ALLOC(g->data->textures, 1);
}

#define add_item(data, list) ({ &(data)->list[(data)->list##_count++]; })

// Create a buffer view and attribute.
static void make_attribute(gltf_t *g, cgltf_buffer_view *buffer_view,
                           cgltf_primitive *primitive,
                           const char *name,
                           cgltf_component_type component_type,
                           cgltf_type type,
                           bool normalized, int count, int ofs,
                           const float v_min[3], const float v_max[3])
{
    cgltf_accessor *accessor;
    cgltf_attribute *attribute;

    accessor = add_item(g->data, accessors);
    accessor->buffer_view = buffer_view;
    accessor->component_type = component_type;
    accessor->offset = ofs;
    accessor->type = type;
    accessor->count = count;
    accessor->normalized = normalized;
    if (v_min) {
        vec3_copy(v_min, accessor->min);
        accessor->has_min = true;
    }
    if (v_max) {
        vec3_copy(v_max, accessor->max);
        accessor->has_max = true;
    }
    attribute = add_item(primitive, attributes);
    attribute->data = accessor;
    attribute->name = strdup(name);
    cgltf_parse_attribute_type(name, &attribute->type, &attribute->index);
}

static int get_material_idx(const image_t *img, const material_t *mat)
{
    int i;
    const material_t *m;
    for (i = 0, m = img->materials; m; m = m->next, i++) {
        if (m == mat) return i;
    }
    return 0;
}

static cgltf_material *save_material(
        gltf_t *g, const material_t *mat, const export_options_t *options)
{
    cgltf_material *material;
    cgltf_pbr_metallic_roughness *pbr;

    material = add_item(g->data, materials);
    material->alpha_cutoff = 0.5;
    material->has_pbr_metallic_roughness = true;
    pbr = &material->pbr_metallic_roughness;
    material->name = strdup(mat->name);
    vec3_copy(mat->emission, material->emissive_factor);
    vec4_copy(mat->base_color, pbr->base_color_factor);
    pbr->metallic_factor = mat->metallic;
    pbr->roughness_factor = mat->roughness;

    if (!options->vertex_color) {
        pbr->base_color_texture.texture = &g->data->textures[0];
        pbr->base_color_texture.scale = 1;
    }
    return material;
}

static cgltf_material *get_default_mat(
        gltf_t *g, const export_options_t *options)
{
    material_t mat;
    if (!g->default_mat) {
        mat = (material_t) {
            .base_color = {1, 1, 1, 1},
            .metallic = 1,
            .roughness = 1,
        };
        g->default_mat = save_material(g, &mat, options);
    }
    return g->default_mat;
}

static void save_layer(gltf_t *g, cgltf_node *root_node,
                       const image_t *img, const layer_t *layer,
                       const palette_t *palette,
                       int palette_pix_size,
                       const export_options_t *options)
{
    volume_mesh_t *mesh;
    cgltf_mesh *gmesh;
    cgltf_node *node;
    cgltf_primitive *primitive;
    cgltf_buffer *buffer;
    cgltf_buffer_view *buffer_view;
    cgltf_accessor *accessor;

    mesh = volume_generate_mesh(
            layer->volume, goxel.rend.settings.effects, palette,
            g_export_options.simplify);

    if (mesh->vertices_count == 0) return;

    gmesh = add_item(g->data, meshes);
    ALLOC(gmesh->primitives, 1);
    primitive = add_item(gmesh, primitives);
    primitive->type = cgltf_primitive_type_triangles;
    ALLOC(primitive->attributes, 3);
    if (layer->material) {
        primitive->material = g->data->materials +
                              get_material_idx(img, layer->material);
    } else {
        primitive->material = get_default_mat(g, options);
    }


    buffer = add_item(g->data, buffers);
    buffer->size = mesh->vertices_count * sizeof(*mesh->vertices);
    buffer->uri = data_new(mesh->vertices, buffer->size, NULL);
    buffer_view = add_item(g->data, buffer_views);
    buffer_view->buffer = buffer;
    buffer_view->size = buffer->size;
    buffer_view->stride = sizeof(*mesh->vertices);
    buffer_view->type = cgltf_buffer_view_type_vertices;

    make_attribute(
            g, buffer_view, primitive,
            "POSITION",
            cgltf_component_type_r_32f,
            cgltf_type_vec3, false,
            mesh->vertices_count, offsetof(typeof(*mesh->vertices), pos),
            mesh->pos_min, mesh->pos_max);
    make_attribute(
            g, buffer_view, primitive,
            "NORMAL",
            cgltf_component_type_r_32f,
            cgltf_type_vec3, false,
            mesh->vertices_count, offsetof(typeof(*mesh->vertices), normal),
            NULL, NULL);
    if (options->vertex_color) {
        make_attribute(g, buffer_view, primitive,
                       "COLOR_0",
                       cgltf_component_type_r_32f,
                       cgltf_type_vec4, false,
                       mesh->vertices_count,
                       offsetof(typeof(*mesh->vertices), color),
                       NULL, NULL);
    } else {
        make_attribute(g, buffer_view, primitive,
                       "TEXCOORD_0",
                       cgltf_component_type_r_32f, cgltf_type_vec2, false,
                       mesh->vertices_count,
                       offsetof(typeof(*mesh->vertices), texcoord),
                       NULL, NULL);
    }

    buffer = add_item(g->data, buffers);
    buffer->size = mesh->indices_count * sizeof(*mesh->indices);
    buffer->uri = data_new(mesh->indices, buffer->size, NULL);
    buffer_view = add_item(g->data, buffer_views);
    buffer_view->buffer = buffer;
    buffer_view->size = buffer->size;
    buffer_view->type = cgltf_buffer_view_type_indices;

    accessor = add_item(g->data, accessors);
    accessor->buffer_view = buffer_view;
    accessor->component_type = cgltf_component_type_r_32u;
    accessor->count = mesh->indices_count;
    accessor->type = cgltf_type_scalar;
    primitive->indices = accessor;

    node = add_item(g->data, nodes);
    node->mesh = gmesh;
    node->name = strdup(layer->name);
    *add_item(root_node, children) = node;

    volume_mesh_free(mesh);
}

static void create_palette_texture(
        gltf_t *g, const image_t *img, int pix_size)
{
    // Create the global palette with all the colors.
    layer_t *layer;
    volume_iterator_t iter;
    int i, s, pos[3], size, x, y, j, k;
    uint8_t c[4];
    uint8_t (*data)[3];
    uint8_t *png;
    cgltf_buffer *buffer;
    cgltf_buffer_view *buffer_view;
    cgltf_image *image;
    cgltf_texture *texture;

    DL_FOREACH(img->layers, layer) {
        iter = volume_get_iterator(layer->volume, 0);
        while (volume_iter(&iter, pos)) {
            volume_get_at(layer->volume, &iter, pos, c);
            palette_insert(&g->palette, c, NULL);
        }
    }

    s = ceil(sqrt(g->palette.size));
    s = max(next_pow2(s), 16);
    s *= pix_size;
    data = calloc(s * s, sizeof(*data));
    // Copy colors as blocks of pix_size x pix_size.
    for (k = 0; k < g->palette.size; k++) {
        x = (k % (s / pix_size)) * pix_size;
        y = (k / (s / pix_size)) * pix_size;
        for (i = 0; i < pix_size; i++) {
            for (j = 0; j < pix_size; j++) {
                memcpy(data[(y + i) * s + x + j],
                        g->palette.entries[k].color, 3);
            }
        }
    }
    png = img_write_to_mem((void*)data, s, s, 3, &size);
    free(data);
    buffer = add_item(g->data, buffers);
    buffer->size = size;
    buffer->uri = data_new(png, size, NULL);
    buffer_view = add_item(g->data, buffer_views);
    buffer_view->buffer = buffer;
    buffer_view->size = size;
    image = add_item(g->data, images);
    image->mime_type = strdup("image/png");
    image->buffer_view = buffer_view;
    texture = add_item(g->data, textures);
    texture->image = image;
    free(png);
}

static void gltf_export(const image_t *img, const char *path,
                        const export_options_t *options)
{
    gltf_t g = {};
    const layer_t *layer;
    cgltf_scene *scene;
    cgltf_node *root_node;
    cgltf_options gltf_options = {};
    material_t *mat;
    const palette_t *palette = NULL;
    const int palette_pix_size = 4;

    gltf_init(&g, options, img);

    if (!options->vertex_color) {
        create_palette_texture(&g, img, palette_pix_size);
        palette = &g.palette;
    }

    DL_FOREACH(img->materials, mat) {
        save_material(&g, mat, options);
    }

    root_node = add_item(g.data, nodes);
    mat4_set((void*)root_node->matrix,
             1, 0,  0, 0,
             0, 0, -1, 0,
             0, 1,  0, 0,
             0, 0,  0, 1);
    root_node->has_matrix = true;
    scene = add_item(g.data, scenes);
    ALLOC(scene->nodes, 1);
    *add_item(scene, nodes) = root_node;

    ALLOC(root_node->children, DL_SIZE(img->layers));
    DL_FOREACH(img->layers, layer) {
        save_layer(&g, root_node, img, layer,
                   palette, palette_pix_size, options);
    }

    cgltf_write_file(&gltf_options, path, g.data);
    cgltf_free(g.data);
    free(g.palette.entries);
}

static int export_as_gltf(const file_format_t *format, const image_t *img,
                          const char *path)
{
    gltf_export(img, path, &g_export_options);
    return 0;
}

static void export_gui(file_format_t *format)
{
    gui_checkbox(_("Vertex Color"), &g_export_options.vertex_color,
                 _("Save colors as vertex attribute"));
    gui_input_float(_("Simplify"), &g_export_options.simplify, 0.1,
                    0, 1, "%.1f");
}

FILE_FORMAT_REGISTER(gltf,
    .name = "gltf",
    .exts = {"*.gltf"},
    .exts_desc = "glTF2",
    .export_gui = export_gui,
    .export_func = export_as_gltf,
    .priority = 100,
)
