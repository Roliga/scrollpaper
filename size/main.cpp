#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>

struct Monitor {
    const unsigned int x, y, width, height;
    const std::string name;

    Monitor(const int x, const int y, const int width, const int height, const char* name)
        : x(x), y(y), width(width), height(height), name(name) {};
};

std::vector<Monitor> getMonitors(Display* display) {
    XRRScreenResources *screens = XRRGetScreenResources(display, DefaultRootWindow(display));
    XRRCrtcInfo *info = NULL;
    XRROutputInfo *output = NULL;
    int i = 0;
    std::vector<Monitor> monitors;

    for (i = 0; i < screens->noutput; i++) {
        output = XRRGetOutputInfo(display, screens, screens->outputs[i]);
        if (output->crtc) {
            info = XRRGetCrtcInfo(display, screens, output->crtc);
            printf("%s: %dx%d+%d+%d\n", output->name, info->width, info->height, info->x, info->y);
            monitors.push_back(Monitor(info->x, info->y, info->width, info->height, output->name));
            XRRFreeCrtcInfo(info);
        }
        XRRFreeOutputInfo(output);
    }
    XRRFreeScreenResources(screens);

    return monitors;
}

int main()
{
    Display *display = XOpenDisplay(NULL);
    XRRScreenResources *screens = XRRGetScreenResources(display, DefaultRootWindow(display));
    XRRCrtcInfo *info = NULL;
    XRROutputInfo *output = NULL;
    int i = 0;

    std::vector<Monitor> monitors;

    for (i = 0; i < screens->noutput; i++) {
        output = XRRGetOutputInfo(display, screens, screens->outputs[i]);
        if (output->crtc) {
            info = XRRGetCrtcInfo(display, screens, output->crtc);
            printf("%s: %dx%d+%d+%d\n", output->name, info->width, info->height, info->x, info->y);
            monitors.push_back(Monitor(info->x, info->y, info->width, info->height, output->name));
            XRRFreeCrtcInfo(info);
        }
        XRRFreeOutputInfo(output);
    }
    XRRFreeScreenResources(screens);

    return 0;
}
