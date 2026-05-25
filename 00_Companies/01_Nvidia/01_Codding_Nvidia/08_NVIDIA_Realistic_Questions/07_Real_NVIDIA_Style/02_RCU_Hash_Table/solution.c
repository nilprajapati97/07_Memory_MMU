// Kernel-space hash table with RCU-protected lookups and spinlock-protected updates
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#define HASH_BITS 8
#define HASH_SIZE (1 << HASH_BITS)

struct rcu_hash_node {
    int key;
    int value;
    struct hlist_node hnode;
};

struct rcu_hash_bucket {
    struct hlist_head head;
    spinlock_t lock;
};

struct rcu_hash_table {
    struct rcu_hash_bucket buckets[HASH_SIZE];
};

static struct rcu_hash_table *rcu_ht_create(void) {
    struct rcu_hash_table *ht = kmalloc(sizeof(*ht), GFP_KERNEL);
    int i;
    for (i = 0; i < HASH_SIZE; ++i) {
        INIT_HLIST_HEAD(&ht->buckets[i].head);
        spin_lock_init(&ht->buckets[i].lock);
    }
    return ht;
}

static unsigned int rcu_hash(int key) {
    return hash_32(key, HASH_BITS);
}

// RCU-protected lookup
static struct rcu_hash_node *rcu_ht_lookup(struct rcu_hash_table *ht, int key) {
    struct rcu_hash_node *node;
    unsigned int idx = rcu_hash(key);
    rcu_read_lock();
    hlist_for_each_entry_rcu(node, &ht->buckets[idx].head, hnode) {
        if (node->key == key) {
            rcu_read_unlock();
            return node;
        }
    }
    rcu_read_unlock();
    return NULL;
}

// Spinlock-protected insert/update
static void rcu_ht_insert(struct rcu_hash_table *ht, int key, int value) {
    struct rcu_hash_node *node;
    unsigned int idx = rcu_hash(key);
    spin_lock(&ht->buckets[idx].lock);
    hlist_for_each_entry(node, &ht->buckets[idx].head, hnode) {
        if (node->key == key) {
            node->value = value;
            spin_unlock(&ht->buckets[idx].lock);
            return;
        }
    }
    node = kmalloc(sizeof(*node), GFP_KERNEL);
    node->key = key;
    node->value = value;
    hlist_add_head_rcu(&node->hnode, &ht->buckets[idx].head);
    spin_unlock(&ht->buckets[idx].lock);
}

// Spinlock-protected delete
static void rcu_ht_delete(struct rcu_hash_table *ht, int key) {
    struct rcu_hash_node *node;
    unsigned int idx = rcu_hash(key);
    spin_lock(&ht->buckets[idx].lock);
    hlist_for_each_entry(node, &ht->buckets[idx].head, hnode) {
        if (node->key == key) {
            hlist_del_rcu(&node->hnode);
            spin_unlock(&ht->buckets[idx].lock);
            synchronize_rcu();
            kfree(node);
            return;
        }
    }
    spin_unlock(&ht->buckets[idx].lock);
}

MODULE_LICENSE("GPL");
