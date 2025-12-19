#include "joystick.h"
#include <linux/joystick.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <vector>

namespace {
int g_fd = -1;
std::vector<int16_t> g_axes;
std::vector<uint8_t> g_buttons;
}

bool js_open(const char* path)
{
    if (g_fd >= 0) return true;
    g_fd = ::open(path, O_RDONLY | O_NONBLOCK);
    if (g_fd < 0) return false;

    // Жёстко не полагаемся на JSIOCGAXES/JSIOCGBUTTONS (не у всех есть),
    // но попробуем получить из ioctl, иначе будем расширять динамически на лету
    unsigned char na = 0, nb = 0;
    if (ioctl(g_fd, JSIOCGAXES, &na) == 0 && na > 0) g_axes.assign(na, 0);
    if (ioctl(g_fd, JSIOCGBUTTONS, &nb) == 0 && nb > 0) g_buttons.assign(nb, 0);
    return true;
}

//БЕСПОЛЕЗНО: функция определена, но нигде не вызывается
/*
void js_close()
{
    if (g_fd >= 0) {
        ::close(g_fd);
        g_fd = -1;
    }
    g_axes.clear();
    g_buttons.clear();
}
*/

static void ensure_axis_size(size_t idx)
{
    if (g_axes.size() <= idx) g_axes.resize(idx + 1, 0);
}

static void ensure_button_size(size_t idx)
{
    if (g_buttons.size() <= idx) g_buttons.resize(idx + 1, 0);
}

bool js_poll()
{
    if (g_fd < 0) return false;
    bool processed = false;
    js_event e;
    while (true) {
        ssize_t r = ::read(g_fd, &e, sizeof(e));
        if (r < (ssize_t)sizeof(e)) break; // нет данных
        processed = true;
        uint8_t type = e.type & ~JS_EVENT_INIT;
        if (type == JS_EVENT_AXIS) {
            ensure_axis_size(e.number);
            g_axes[e.number] = e.value;
        } else if (type == JS_EVENT_BUTTON) {
            ensure_button_size(e.number);
            g_buttons[e.number] = (e.value != 0);
        }
    }
    return processed;
}

bool js_get_axis(int index, int16_t& outValue)
{
    if (index < 0) return false;
    size_t idx = static_cast<size_t>(index);
    if (idx >= g_axes.size()) return false;
    outValue = g_axes[idx];
    return true;
}

int js_num_axes()
{
    return static_cast<int>(g_axes.size());
}

int js_num_buttons()
{
    return static_cast<int>(g_buttons.size());
}


