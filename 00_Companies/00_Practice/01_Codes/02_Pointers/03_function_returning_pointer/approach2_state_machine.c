/* Approach 2: State Machine Pattern */
#include <stdio.h>

// State function type
typedef void (*state_func_t)(void);

// State functions
void state_idle(void);
void state_running(void);
void state_stopped(void);

// Current state
state_func_t current_state = state_idle;

// Function that returns next state
state_func_t (*get_next_state(int event))(void) {
    if (current_state == state_idle && event == 1) {
        return state_running;
    } else if (current_state == state_running && event == 2) {
        return state_stopped;
    } else if (current_state == state_stopped && event == 3) {
        return state_idle;
    }
    return current_state;
}

void state_idle(void) {
    printf("State: IDLE\n");
}

void state_running(void) {
    printf("State: RUNNING\n");
}

void state_stopped(void) {
    printf("State: STOPPED\n");
}

int main() {
    current_state();
    
    current_state = get_next_state(1);
    current_state();
    
    current_state = get_next_state(2);
    current_state();
    
    current_state = get_next_state(3);
    current_state();
    
    return 0;
}
