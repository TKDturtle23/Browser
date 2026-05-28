#include "Platform_XCB.h"

#ifdef __linux__

#include <cstdlib>
#include <cstring>

Platform_XCB::~Platform_XCB() {
    CloseWindow();
}

bool Platform_XCB::OpenWindow(
    int width,
    int height,
    const char* title
) {
    windowWidth = width;
    windowHeight = height;

    connection = xcb_connect(nullptr, nullptr);

    if (xcb_connection_has_error(connection)) {
        return false;
    }

    const xcb_setup_t* setup =
        xcb_get_setup(connection);

    xcb_screen_iterator_t iterator =
        xcb_setup_roots_iterator(setup);

    screen = iterator.data;

    window = xcb_generate_id(connection);

    uint32_t mask =
        XCB_CW_BACK_PIXEL |
        XCB_CW_EVENT_MASK;

    uint32_t values[] = {
        screen->black_pixel,

        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_KEY_PRESS |
        XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_POINTER_MOTION
    };

    xcb_create_window(
        connection,
        XCB_COPY_FROM_PARENT,
        window,
        screen->root,
        0,
        0,
        width,
        height,
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        screen->root_visual,
        mask,
        values
    );

    graphicsContext =
        xcb_generate_id(connection);

    uint32_t gcValues[] = {
        screen->white_pixel,
        screen->black_pixel
    };

    xcb_create_gc(
        connection,
        graphicsContext,
        window,
        XCB_GC_FOREGROUND |
        XCB_GC_BACKGROUND,
        gcValues
    );

    xcb_change_property(
        connection,
        XCB_PROP_MODE_REPLACE,
        window,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,
        std::strlen(title),
        title
    );

    xcb_map_window(connection, window);

    xcb_flush(connection);

    running = true;

    return true;
}

void Platform_XCB::CloseWindow() {

    if (!connection)
        return;

    xcb_disconnect(connection);

    connection = nullptr;

    running = false;
}

void Platform_XCB::Present(
    const std::vector<Color>& pixels
) {

    if (!connection)
        return;

    xcb_put_image(
        connection,
        XCB_IMAGE_FORMAT_Z_PIXMAP,
        window,
        graphicsContext,
        windowWidth,
        windowHeight,
        0,
        0,
        0,
        screen->root_depth,
        pixels.size() * sizeof(Color),
        reinterpret_cast<const uint8_t*>(pixels.data())
    );

    xcb_flush(connection);
}

bool Platform_XCB::PollEvent(
    Event& event
) {

    if (!connection)
        return false;

    xcb_generic_event_t* xcbEvent =
        xcb_poll_for_event(connection);

    if (!xcbEvent)
        return false;

    uint8_t type =
        xcbEvent->response_type & ~0x80;

    switch (type) {

        case XCB_CLIENT_MESSAGE: {
            event.type = EventType::Quit;
            running = false;
            break;
        }

        case XCB_DESTROY_NOTIFY: {
            event.type = EventType::Quit;
            running = false;
            break;
        }

        case XCB_CONFIGURE_NOTIFY: {

            auto* resizeEvent =
                reinterpret_cast<
                    xcb_configure_notify_event_t*
                >(xcbEvent);

            windowWidth = resizeEvent->width;
            windowHeight = resizeEvent->height;

            event.type = EventType::Resize;
            event.width = windowWidth;
            event.height = windowHeight;

            break;
        }

        case XCB_KEY_PRESS: {

            auto* keyEvent =
                reinterpret_cast<
                    xcb_key_press_event_t*
                >(xcbEvent);

            event.type = EventType::KeyPress;
            event.key = keyEvent->detail;

            break;
        }

        case XCB_KEY_RELEASE: {

            auto* keyEvent =
                reinterpret_cast<
                    xcb_key_release_event_t*
                >(xcbEvent);

            event.type = EventType::KeyRelease;
            event.key = keyEvent->detail;

            break;
        }

        case XCB_MOTION_NOTIFY: {

            auto* motionEvent =
                reinterpret_cast<
                    xcb_motion_notify_event_t*
                >(xcbEvent);

            event.type = EventType::MouseMove;
            event.x = motionEvent->event_x;
            event.y = motionEvent->event_y;

            break;
        }

        case XCB_BUTTON_PRESS: {

            event.type = EventType::MouseButtonPress;

            break;
        }

        case XCB_BUTTON_RELEASE: {

            event.type = EventType::MouseButtonRelease;

            break;
        }

        default:
            event.type = EventType::None;
            break;
    }

    free(xcbEvent);

    return true;
}

int Platform_XCB::GetWidth() const {
    return windowWidth;
}

int Platform_XCB::GetHeight() const {
    return windowHeight;
}

bool Platform_XCB::IsRunning() const {
    return running;
}

#endif