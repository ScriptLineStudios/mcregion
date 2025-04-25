#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#define NBT_IMPLEMENTATION
#include "libnbt/nbt.h"

#include "cJSON/cJSON.h"

void usage() {
    printf("Usage: ./mcregion <region_file>.mca\n");
    exit(1);
}

static size_t reader_read(void* userdata, uint8_t* data, size_t size) {
    memcpy(data, userdata, size);
    return NBT_BUFFER_SIZE;
}

static void print_nbt_tree(nbt_tag_t* tag, int indentation) {
  for (int i = 0; i < indentation; i++) {
    printf(" ");
  }

  if (tag->name) {
    printf("%s: ", tag->name);
  }

  switch (tag->type) {
    case NBT_TYPE_END: {
      printf("[end]");
      break;
    }
    case NBT_TYPE_BYTE: {
      printf("%hhd", tag->tag_byte.value);
      break;
    }
    case NBT_TYPE_SHORT: {
      printf("%hd", tag->tag_short.value);
      break;
    }
    case NBT_TYPE_INT: {
      printf("%d", tag->tag_int.value);
      break;
    }
    case NBT_TYPE_LONG: {
      printf("%ld", tag->tag_long.value);
      break;
    }
    case NBT_TYPE_FLOAT: {
      printf("%f", tag->tag_float.value);
      break;
    }
    case NBT_TYPE_DOUBLE: {
      printf("%f", tag->tag_double.value);
      break;
    }
    case NBT_TYPE_BYTE_ARRAY: {
      printf("[byte array]");
      break;
      for (size_t i = 0; i < tag->tag_byte_array.size; i++) {
        printf("%hhd ", tag->tag_byte_array.value[i]);
      }
      break;
    }
    case NBT_TYPE_STRING: {
      printf("%s", tag->tag_string.value);
      break;
    }
    case NBT_TYPE_LIST: {
      printf("\n");
      for (size_t i = 0; i < tag->tag_list.size; i++) {
        print_nbt_tree(tag->tag_list.value[i], indentation + tag->name_size + 2);
      }
      break;
    }
    case NBT_TYPE_COMPOUND: {
      printf("\n");
      for (size_t i = 0; i < tag->tag_compound.size; i++) {
        print_nbt_tree(tag->tag_compound.value[i], indentation + tag->name_size + 2);
      }
      break;
    }
    case NBT_TYPE_INT_ARRAY: {
      printf("[int array]");
      break;
      for (size_t i = 0; i < tag->tag_int_array.size; i++) {
        printf("%d ", tag->tag_int_array.value[i]);
      }
      break;
    }
    case NBT_TYPE_LONG_ARRAY: {
      printf("[long array]");
      break;
      for (size_t i = 0; i < tag->tag_long_array.size; i++) {
        printf("%ld ", tag->tag_long_array.value[i]);
      }
      break;
    }
    default: {
      printf("[error]");
    }
  }

  printf("\n");
}

int int_div(int a, int b) {
  if (a >= 0) {
    return (int)floor((float)a / (float)b);
  }
  return (int)floor((float)a / (float)b);
}

int mod(int a, int b) {
  return ((a % b) + b) % b;
}

typedef struct {
  nbt_tag_t *tags;
  int x, z;
} Chunk;

typedef struct {
  Chunk *chunks;
  size_t num_chunks;
  int x, z;
} Region;

void get_block(Region *region, Chunk *chunk, int x, int y, int z) {
  nbt_tag_t *final_section;
  nbt_tag_t *section;
  nbt_tag_t *tags = chunk->tags;
  nbt_tag_t *sections = nbt_tag_compound_get(tags, "sections");
  
  size_t index = 0;
  while (1) {
      section = nbt_tag_list_get(sections, index);
      nbt_tag_t *y_level = nbt_tag_compound_get(section, "Y");
      int y_val = y_level[0].tag_byte.value;
      if (y_val == int_div(y, 16)) {
          final_section = section;
          break;
      }
      index++;
  }

  size_t section_index = mod(y, 16) * 16*16 + z * 16 + x;
  nbt_tag_t *bstates = nbt_tag_compound_get(final_section, "block_states");
  if (!bstates) {
    return; 
  }
  nbt_tag_t *data = nbt_tag_compound_get(bstates, "data");
  if (!data) {
    return; 
  }
  int64_t *states = data[0].tag_long_array.value;

  nbt_tag_t *palette = nbt_tag_compound_get(bstates, "palette");

  size_t length = palette->tag_list.size - 1;
  size_t bit_length = (int)floor((int)log2(length)) + 1;

  size_t bits = (size_t)fmax(bit_length, 4);
  size_t state = int_div(section_index, int_div(64, bits));
  int64_t block_data = states[state];

  if (block_data < 0) {
      block_data += pow(2, 64);
  }

  size_t shifted_data = block_data >> (mod(section_index, (int)(int_div(64, bits))) * bits);

  size_t palette_id = shifted_data & ((int)(pow(2, bits)) - 1);

  nbt_tag_t *block = nbt_tag_list_get(palette, palette_id);
  // char *name = block->tag_string.value;
  // if (x == 14) {
    // printf("here\n");
  // print_nbt_tree(block, -1); 
  nbt_tag_t *name = nbt_tag_compound_get(block, "Name");

  // printf("%s\n", name->tag_string.value);
  if (!strcmp(name->tag_string.value, "minecraft:dark_oak_sign")) {
    int real_x = x + 16 * chunk->x + 512 * region->x;
    int real_z = z + 16 * chunk->z + 512 * region->z;

    printf("found sign at: (%d %d %d)\n", real_x, y, real_z);
  }

  // printf("%d\n", strcmp(name->tag_string.value, "minecraft:air"));
  // exit(1);
  // }
  // print_nbt_tree(block, -1); 
}

Region parse(const char *filename) {
    char filename_copy[256];
    strcpy(filename_copy, filename);
    char *pch = strtok ((char *)filename_copy, "/");
    char *prev;
    while (pch != NULL) {
      prev = pch;
      pch = strtok (NULL, "/");
    }
    char name_buffer[32];
    strcpy(name_buffer, prev+2);
    printf("%s\n", name_buffer);
    int rx = atoi(strtok((char *)name_buffer, "."));
    int rz = atoi(strtok(NULL, "."));
    printf("done\n");

    FILE *file = fopen(filename, "rb");
    fseek(file, 0L, SEEK_END);
    size_t num_bytes = ftell(file);
    fseek(file, 0L, SEEK_SET);	    
    unsigned char *buffer = (unsigned char*)calloc(num_bytes, sizeof(unsigned char));	
    size_t size = fread(buffer, sizeof(char), num_bytes, file);
    fclose(file);
    (void) size;

    Region region;
    region.x = rx;
    region.z = rz;
    region.num_chunks = 0;
    region.chunks = (Chunk *)malloc(sizeof(Chunk) * region.num_chunks);

    //this format is fucking stupid
    for (int cx = 0; cx < 32; cx++) {
      for (int cz = 0; cz < 32; cz++) {
        size_t byte_offset = 4 * ((cx & 31) + (cz & 31) * 32);
        // printf("bo: %ld\n", byte_offset);
        int offset =  (0x0) | (buffer[byte_offset+0] << 16) | (buffer[byte_offset+1] << 8) | (buffer[byte_offset+2]);
        char sectors = buffer[byte_offset+3];
        // printf("%d %u\n", offset, sectors);
        if (offset == 0 && sectors == 0) {
            goto next;
        }

        offset = offset * 4096; //skipping the header
        
        int length =  (buffer[offset] << 24) | (buffer[offset+1] << 16) | (buffer[offset+2] << 8) | (buffer[offset+3]);
        // char compression = buffer[offset + 4];
        uint8_t *bytes = malloc(NBT_BUFFER_SIZE);
        // printf("%d %d\n", cx, cz);
        for (int i = 0; i < length - 1; i++) {
            bytes[i] = buffer[offset + 5 + i];
            // printf("%d ", bytes[i]);
        }
        // exit(1);
        // printf("length: %d compression: %u first_byte: %u\n", length, compression, bytes[length-2]);
        
        nbt_reader_t reader;
        reader.read = reader_read;
        reader.userdata = bytes;

        nbt_tag_t *tags = nbt_parse(reader, 2);
        free(bytes);

        Chunk chunk;
        chunk.tags = tags;
        chunk.x = cx;
        chunk.z = cz;
        // printf("Writing %d %d to region\n", cx, cz);
        region.num_chunks++;
        region.chunks = realloc(region.chunks, sizeof(Chunk) * region.num_chunks);
        region.chunks[region.num_chunks - 1] = chunk;
        
        next:
        ;
      }
    }

    free(buffer);
    return region;
    // printf("%d\n", tags[1].type);
    // print_nbt_tree(tags, -1);

    // int x = 15;
    // int y = 80;
    // int z = 15;

    // for (y = -64; y < 300; y++) {
    //   for (x = 0; x < 16; x++) {
    //     for (z = 0; z < 16; z++) {
    //       // {{{
    //       // printf("%d %d %d\n", x, y, z);
    //       get_block(tags, x, y, z);
    //     }
    //   }
    // }
}

void get_block_data(const char *filename) {
  Region region = parse(filename);
  
  for (size_t i = 0; i < region.num_chunks; i++) {
      Chunk chunk = region.chunks[i];
      for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
          for (int y = -64; y < 320; y++) {
            get_block(&region, &chunk, x, y, z);
          }
        }
      }
      nbt_free_tag(chunk.tags);
  }
}

void get_sign_data(const char *filename) {
    Region region = parse(filename);

    for (size_t i = 0; i < region.num_chunks; i++) {
      Chunk chunk = region.chunks[i];
      nbt_tag_t *tags = chunk.tags;
      nbt_tag_t *block_entities = nbt_tag_compound_get(tags, "block_entities");

      for (size_t i = 0; i < block_entities->tag_list.size; i++) {
        nbt_tag_t *tag = nbt_tag_list_get(block_entities, i);

        nbt_tag_t *id = nbt_tag_compound_get(tag, "id");
        char *block_id = id->tag_string.value;
        if (strcmp(block_id, "minecraft:sign") == 0) {
          int x = nbt_tag_compound_get(tag, "x")->tag_int.value;
          int y = nbt_tag_compound_get(tag, "y")->tag_int.value;
          int z = nbt_tag_compound_get(tag, "z")->tag_int.value;
          // nbt_tag_t *inner = nbt_tag_compound_get(tag, "front_text");
          nbt_tag_t *front_text = nbt_tag_compound_get(tag, "front_text");
          // printf("--- TAG START ---\n");
          // print_nbt_tree(tag, 0);
          if (front_text) {
            nbt_tag_t *messages = nbt_tag_compound_get(front_text, "messages");
            for (size_t i = 0; i < 4; i++) {
              const char *value = messages->tag_compound.value[i]->tag_string.value;
              cJSON *json = cJSON_Parse(value);
              char *text = cJSON_GetObjectItemCaseSensitive(json, "text")->valuestring;
              if (strcmp(text, "") == 0) {
                cJSON *array = cJSON_GetObjectItemCaseSensitive(json, "extra");
                if (array != NULL) {
                  const char *t = cJSON_GetObjectItemCaseSensitive(array->child, "text")->valuestring;
                  printf("%s ", t);
                }
              }
              else {
                printf("%s ", text);
              }
            }
            puts("");
          }
          else {
            const char *lookups[4] = {"Text1", "Text2", "Text3", "Text4"};
            for (size_t j = 0; j < 4; j++) {
              nbt_tag_t *inner = nbt_tag_compound_get(tag, lookups[j]);
              if (inner->type == NBT_TYPE_STRING) {
                char *value = inner->tag_string.value;
                cJSON *json = cJSON_Parse(value);
                char *text = cJSON_GetObjectItemCaseSensitive(json, "text")->valuestring;
                if (strcmp(text, "") == 0) {
                  cJSON *array = cJSON_GetObjectItemCaseSensitive(json, "extra");
                  if (array != NULL) {
                    const char *t = cJSON_GetObjectItemCaseSensitive(array->child, "text")->valuestring;
                    printf("%s ", t);
                  }
                }
                else {
                  printf("%s ", text);
                }
              }
            }
            puts("");
          }
          // printf("--- TAG ENDs ---\n");
        }
      }
      nbt_free_tag(tags);
    }
    free(region.chunks);
}

int main(void) {
    struct dirent *de; 
    DIR *dr = opendir("/home/scriptline/.minecraft/saves/constantiam world download 2/region/"); 
  
    if (dr == NULL) { 
        printf("Could not open current directory" ); 
        return 0; 
    } 
  
    printf("starting...\n");
    short k = 0;
    while ((de = readdir(dr)) != NULL) {
      if (k > 1) {
        char buffer[1024];
        sprintf(buffer, "/home/scriptline/.minecraft/saves/constantiam world download 2/region/%s", de->d_name);
        // get_block_data(buffer);
        get_sign_data(buffer);
        printf("done: %d\n", k);
        // exit(1);
      }
      k++;
    }
    closedir(dr);     
    return 0;
}

