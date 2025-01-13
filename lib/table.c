#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "table.h"

#define TABLE_MAX_LOAD 0.75

void init_table(Table* table){
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void free_table(Table* table){
    FREE_ARRAY(Entry, table->entries, table->capacity);
    init_table(table);
}

bool table_set(Table* table, ObjString* key, Value value){
    if(table->count + 1 > table->capacity * TABLE_MAX_LOAD){
        int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table,capacity);
    }

    Entry* entry = find_entry(table->entries, table->capacity,key);
    bool is_new_key = entry->key == NULL;

    /* 
        count = NUMBER_OF_ENTRIES + TOMBSTONES
        which is why we only increase the count of the table
        if the entry was not a tombstone, this is how we keep the 
        load factor in control
    */
    if(is_new_key && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return is_new_key;
}

static Entry* find_entry(Entry* entries, int capacity,
                                         ObjString* key){
    /*this is how a bucket is found, the modulus*/
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;
    for(;;){
        Entry* entry = &entries[index];
        if(entry->key == key || entry->key == NULL){
            return entry;
        }

        if(entry->key == NULL){
            if(IS_NIL(entry->value)){
                /*
                    we want to make use of the tombstone incase there's one
                */
                return tombstone != NULL ? tombstone : entry; 
            }else {
                tombstone = entry;
            }
        }else if(entry->key == key){
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

static void adjust_capacity(Table* table, int capacity){
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++){
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    /*
        we reset the table count because we probably have
        tombstones, while rebuilding the table we do not need them
        so the count is probably inflated
    */
    table->count = 0;
    for (int i = 0; i < table->capacity; i++){
        Entry* entry = &table->entries[i];
        if(entry->key == NULL) continue;

        /*
            here is abit confusing but what happens is;

            by passing the new array (entries) instead of table->entries

            we get access to a pointer into entries thru find_entry() [dest]
            then we set the old entry (entry) to the new bucket (dest)

            hence writing to the new array (entries)

        */

        /*
            here we are looking for the bucket where a certain key is supposed to 
            go in the new array

            hence the rebuilding 
        */
        Entry* dest = find_entry(entries,capacity,entry->key);
        dest->key = entry->key;
        dest->value = entry->value;

        /*only increment the count if we actually add an entry*/
        table->count++;
    }
    
    FREE_ARRAY(Entry, table->entries,table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

void table_add_all(Table* from, Table* to){
    for (int i = 0; i < from->capacity; i++){
        Entry* entry = &from->entries[i];
        if(entry->key != NULL){
            table_set(to, entry->key, entry->value);
        }
    }
}

bool table_get(Table* table, ObjString* key, Value* value){
    if(table->count == 0) return false;

    Entry* entry = find_entry(table->entries,table->capacity,key);

    if(entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

bool table_delete(Table* table, ObjString* key){
    if(table->count == 0) return false;

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if(entry->key == NULL) return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

ObjString* table_find_string(Table* table, const char* chars, int length, uint32_t hash){
    if(table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;
    for(;;){
        Entry* entry = &table->entries[index];

        if(entry->key == NULL){
            /*
                the entry is genuinely NULL
            */
            if(IS_NIL(entry->value)) return NULL;
        }else if (entry->key->length == length 
                    && entry->key->hash == hash
                    && memcmp(entry->key->chars, chars, length) == 0){
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}