
#define DEFINE_LINKED_HASH(type, name) \
    struct hash_item_##name { \
        type data; \
        int key; \
        struct hash_item_##name* next; \
    }; \
    struct linked_hash_##name { \
        struct hash_item_##name* head; \
        struct hash_item_##name* tail; \
        int max_size; \
    };

#define LINKED_HASH(name) \
    struct linked_hash_##name

#define LINKED_HASH_GET_HEAD(hash) \
    &hash.head->data;

#define LINKED_HASH_INIT(hash, size) \
    hash.head = malloc(sizeof(*hash.head)); \
    memset(hash.head, 0, (int)sizeof(*hash.head)); \
    if (hash.head == NULL) { \
        fprintf(stderr, "ERROR: malloc returned error\n%s:%d - %s\n", \
            __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
    hash.head->next = NULL; \
    hash.tail = hash.head; \
    hash.max_size = size;

#define LINKED_HASH_DESTROY(hash, name) \
    if (hash.head != NULL) { \
        struct hash_item_##name* curr = hash.head; \
        while (curr != NULL) { \
            curr = hash.head->next; \
            free(hash.head); \
            hash.head = curr; \
        } \
    }
