#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/events/behavior_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/kscan.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);


// &ski (Sticky Key Infinite) — a drop-in replacement for &sk that:
// - Acts like "&sk" but has no time-based release
// - Releases on any other &kp key
// - Doesn't release on modifiers/layers

// Track last sticky key
static struct {
    struct zmk_behavior_binding binding;
    struct zmk_behavior_binding_event event;
    bool active;
} ski_state = {
    .active = false,
};

// Called when &ski is pressed
static int ski_press(struct zmk_behavior_binding *binding,
                     struct zmk_behavior_binding_event event) {
    LOG_DBG("SKI press at pos %d", event.position);

    // If another sticky key is active, release it
    if (ski_state.active) {
        behavior_keymap_binding_released(&ski_state.binding, ski_state.event);
        ski_state.active = false;
    }

    // Press this one and store state
    int err = behavior_keymap_binding_pressed(binding, event);
    if (err) {
        LOG_ERR("Failed to press SKI binding");
        return err;
    }

    ski_state.binding = *binding;
    ski_state.event = event;
    ski_state.active = true;

    return ZMK_BEHAVIOR_OPAQUE;
}

// Called when &ski is released — do nothing (it’s sticky)
static int ski_release(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

// Called for *all* key events — even ones not using &ski
static int ski_event_listener(const zmk_event_t *eh) {
    if (!ski_state.active) {
        return 0;
    }

    const struct zmk_behavior_binding_event *event =
        as_zmk_behavior_binding_event(eh);
    if (!event) {
        return 0;
    }

    // Ignore repeated press of same key (e.g., holding)
    if (event->position == ski_state.event.position) {
        return 0;
    }

    // Get behavior at this position
    const struct zmk_behavior_binding *binding =
        zmk_keymap_behavior_binding_for_position(event->layer, event->position);
    if (!binding) {
        return 0;
    }

    // If it's not &kp, ignore (only release on alpha keys)
    if (strcmp(binding->behavior_dev->name, "behaviors.kp") != 0) {
        return 0;
    }

    // Release sticky key
    behavior_keymap_binding_released(&ski_state.binding, ski_state.event);
    ski_state.active = false;

    return 0;
}

ZMK_LISTENER(ski_listener, ski_event_listener);
ZMK_SUBSCRIPTION(ski_listener, zmk_behavior_binding_pressed);

static const struct behavior_driver_api ski_driver_api = {
    .binding_pressed = ski_press,
    .binding_released = ski_release,
};

static int ski_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

DEVICE_DEFINE(ski_behavior, "ski_behavior", ski_init, NULL, NULL, NULL,
              POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
              &ski_driver_api);
