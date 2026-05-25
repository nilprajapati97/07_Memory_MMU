#!/bin/bash

# Complete C Interview Preparation Structure Generator
# This script creates the directory structure for all topics

BASE_DIR="/home/nilprajapti/My_Scrach/09_Interviews/00_Preparation/C/Practice"

echo "Creating complete directory structure..."

# 2. Pointers (12 questions)
mkdir -p "$BASE_DIR/02_Pointers"/{01_const_pointers,02_function_pointers,03_function_returning_pointer,04_pointer_array_declarations,05_pointer_types,06_memory_functions,07_string_functions,08_string_conversion,09_2d_array_passing,10_dynamic_2d_3d_arrays,11_malloc_calloc_realloc,12_custom_allocator}

# 3. Linked Lists (15 questions)
mkdir -p "$BASE_DIR/03_Linked_Lists"/{01_singly_linked_list,02_reverse_list,03_detect_loop,04_find_remove_loop,05_find_middle,06_nth_from_end,07_merge_sorted,08_sort_list,09_palindrome_list,10_intersection_point,11_doubly_linked_list,12_circular_linked_list,13_kernel_style_list,14_delete_node,15_swap_pairs}

# 4. Strings (10 questions)
mkdir -p "$BASE_DIR/04_Strings"/{01_reverse_string,02_palindrome,03_anagram,04_remove_duplicates,05_first_non_repeating,06_count_chars,07_permutations,08_longest_common,09_pattern_matching,10_replace_spaces}

# 5. Arrays (14 questions)
mkdir -p "$BASE_DIR/05_Arrays"/{01_find_largest,02_reverse_array,03_rotate_array,04_missing_number,05_duplicate_number,06_majority_element,07_kadane_algorithm,08_merge_sorted_arrays,09_remove_duplicates,10_binary_search,11_pair_sum,12_matrix_rotation,13_spiral_traversal,14_transpose_matrix}

# 6. Recursion (7 questions)
mkdir -p "$BASE_DIR/06_Recursion"/{01_factorial_fibonacci,02_gcd_lcm,03_tower_of_hanoi,04_power_function,05_reverse_recursion,06_subsets,07_n_queens}

# 7. Memory & Storage (14 questions)
mkdir -p "$BASE_DIR/07_Memory_Storage"/{01_stack_vs_heap,02_storage_classes,03_volatile_keyword,04_memory_layout,05_sizeof_operator,06_struct_padding,07_bit_fields,08_union_vs_struct,09_define_vs_const,10_typedef_vs_define,11_inline_vs_macros,12_static_keyword,13_extern_keyword,14_include_guards}

# 8. OS/Kernel/Concurrency (20 questions)
mkdir -p "$BASE_DIR/08_OS_Kernel_Concurrency"/{01_ring_buffer,02_producer_consumer,03_thread_safe_queue,04_spinlock,05_semaphore,06_reader_writer_lock,07_deadlock,08_mutex_semaphore_spinlock,09_atomic_counter,10_memory_barriers,11_malloc_free_impl,12_memcpy_optimized,13_state_machine,14_scheduler,15_container_of,16_offsetof,17_kernel_linked_list,18_timer_wheel,19_reentrant_functions,20_signal_handler}

# 9. Stack & Queue (7 questions)
mkdir -p "$BASE_DIR/09_Stack_Queue"/{01_stack_array_list,02_queue_array_list,03_stack_queue_conversion,04_circular_queue,05_min_stack,06_postfix_evaluation,07_balanced_parentheses}

# 10. Tree/Graph (7 questions)
mkdir -p "$BASE_DIR/10_Tree_Graph"/{01_tree_traversals,02_level_order,03_height_depth,04_lca,05_check_bst,06_mirror_tree,07_dfs_bfs}

# 11. Embedded Gotchas (10 questions)
mkdir -p "$BASE_DIR/11_Embedded_Gotchas"/{01_safe_min_macro,02_seconds_per_year,03_infinite_loop,04_hardware_register,05_complex_declarations,06_extern_c,07_array_pointer_equivalence,08_strict_aliasing,09_undefined_behavior,10_local_variable_return}

# 12. Algorithms/DSA (5 questions)
mkdir -p "$BASE_DIR/12_Algorithms_DSA"/{01_sorting_algorithms,02_binary_search_variants,03_hash_table,04_lru_cache,05_trie}

# 13. Number/Math (7 questions)
mkdir -p "$BASE_DIR/13_Number_Math"/{01_prime_numbers,02_special_numbers,03_reverse_digits,04_sum_of_digits,05_base_conversion,06_square_root,07_float_print}

# Top 20 Must-Prepare
mkdir -p "$BASE_DIR/00_Top20_MustPrepare"

echo "✅ Directory structure created successfully!"
echo ""
echo "Total structure:"
echo "  - 13 main topics"
echo "  - 143 question directories"
echo "  - Ready for implementations"
