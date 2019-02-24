#include <iostream>
#include <algorithm>
#include <SFML/Graphics.hpp>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include "util.cpp"
#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <libconfig.h++>

struct Monitor {
    const unsigned int x, y, width, height;
    const std::string name;

    Monitor(const int x, const int y, const int width, const int height, const char* name)
        : x(x), y(y), width(width), height(height), name(name) {};
};

std::vector<Monitor> getMonitors() {
    Display *display = XOpenDisplay(NULL);
    XRRScreenResources *screens = XRRGetScreenResources(display, DefaultRootWindow(display));
    XRRCrtcInfo *info = NULL;
    XRROutputInfo *output = NULL;

    std::vector<Monitor> monitors;
    int i = 0;

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
    XCloseDisplay(display);

    return monitors;
}

std::string getConfigDir() {
    std::string configDir;

    if(const char* env = std::getenv("XDG_CONFIG_HOME")) {
        configDir = env;
    }
    else if(const char* env = std::getenv("HOME")) {
        configDir = env;
        configDir.append("/.config");
    }
    else {
        throw "Unable to find config directory";
    }

    configDir.append("/scrollpaper/");

    return configDir;
}

class WallpaperWindow {
    sf::RenderWindow window;
    sf::Texture picTexture;
    sf::Sprite picSprite;
    sf::Vector2f picOffset;
    sf::Vector2f picOffsetMin;
    Display* display;
    libconfig::Config windowConfig;
    std::string windowConfigFilePath;

    public:
    WallpaperWindow(const Monitor monitor, const std::string wallpaperPath) :
        picOffset(sf::Vector2f(0, 0))
    {
        display = XOpenDisplay(NULL);

        // Get the default screen
        int screen = DefaultScreen(display);

        // Let's create the main window
        XSetWindowAttributes attributes;
        attributes.background_pixel = BlackPixel(display, screen);
        attributes.event_mask       = ExposureMask;
        Window win = XCreateWindow(display, RootWindow(display, screen),
                monitor.x, monitor.y, monitor.width, monitor.height, 0,
                DefaultDepth(display, screen),
                InputOutput,
                DefaultVisual(display, screen),
                CWBackPixel | CWEventMask, &attributes);
        //if (!window)
        //	return EXIT_FAILURE;

        XStoreName(display, win , "SFML Window");

        // Use a separate sub-window for mouse input since SFML steals all input in the main window
        XSetWindowAttributes attributes2;
        attributes2.event_mask       = ButtonPressMask | ButtonMotionMask | ButtonReleaseMask;
        Window inputWin = XCreateWindow(display, win,
                0, 0, monitor.width, monitor.height, 0,
                CopyFromParent,
                InputOnly,
                0,
                CWEventMask, &attributes2);

        x11_window_set_desktop(display, win);
        x11_window_set_borderless(display, win);
        x11_window_set_below(display, win);
        x11_window_set_sticky(display, win);
        x11_window_set_skip_taskbar(display, win);
        x11_window_set_skip_pager(display, win);

        XMapWindow(display, win);
        XMapWindow(display, inputWin);
        XFlush(display);

        window.create(win);

        sf::View view;
        view.reset(sf::FloatRect(0, 0, monitor.width, monitor.height));
        window.setView(view);

        picTexture.loadFromFile(wallpaperPath);
        picTexture.setSmooth(true);
        picSprite = sf::Sprite(picTexture);

        sf::Vector2f picSize = (sf::Vector2f)picTexture.getSize();
        sf::Vector2f windowSize = (sf::Vector2f)window.getSize();

        if (windowSize.x / windowSize.y > picSize.x / picSize.y) {
            picSprite.setScale(windowSize.x / picSize.x, windowSize.x / picSize.x);
            picOffsetMin = sf::Vector2f(0, -(picSprite.getGlobalBounds().height - windowSize.y));
        }
        else {
            picSprite.setScale(windowSize.y / picSize.y, windowSize.y / picSize.y);
            picOffsetMin = sf::Vector2f(windowSize.x - picSprite.getLocalBounds().width, 0);
            picOffsetMin = sf::Vector2f(-(picSprite.getGlobalBounds().width - windowSize.x), 0);
        }

        windowConfigFilePath = getConfigDir() + "/position_" + monitor.name + ".cfg";

        try {
            // Try to load and validate config
            windowConfig.readFile(windowConfigFilePath.c_str());

            float x = windowConfig.getRoot().lookup("position.x");
            float y = windowConfig.getRoot().lookup("position.y");

            if (x > 0 || y > 0 || x < picOffsetMin.x || y < picOffsetMin.y)
                throw 0;

            picOffset = sf::Vector2f(x, y);
            picSprite.setPosition(picOffset);
        }
        catch(...) {
            // Initialize fresh config
            windowConfig.clear();

            libconfig::Setting &root = windowConfig.getRoot();
            root.add("position", libconfig::Setting::TypeGroup);

            libconfig::Setting &position = root["position"];
            position.add("x", libconfig::Setting::TypeFloat);
            position.add("y", libconfig::Setting::TypeFloat);

            // Center picture by default
            picOffset = picOffsetMin / 2.0f;
            picSprite.setPosition(picOffset);

            position["x"] = picOffset.x;
            position["y"] = picOffset.y;
        }
    }

    void updateLoop() {
        sf::Vector2f mouseDragOld;
        sf::Vector2f mouseDragNew;
        bool mouseDragging = false;
        //sf::Clock clock;
        libconfig::Setting &positionSetting = windowConfig.lookup("position");

        while (window.isOpen())
        {
            XEvent event;
            XNextEvent(display, &event);

            switch (event.type) {
                case Expose:
                    window.draw(picSprite);
                    window.display();
                    break;
                case ButtonPress:
                    // Mouse button is pressed, get the position and set moving as active
                    if (event.xbutton.button == Button1) {
                        mouseDragging = true;
                        mouseDragOld = sf::Vector2f(event.xbutton.x, event.xbutton.y);
                    }
                    break;
                case ButtonRelease:
                    // Mouse button is released, no longer move
                    if (event.xbutton.button == Button1) {
                        mouseDragging = false;

                        // Save offset config
                        try {
                            positionSetting["x"] = picOffset.x;
                            positionSetting["y"] = picOffset.y;
                            windowConfig.writeFile(windowConfigFilePath.c_str());
                        }
                        catch (const libconfig::FileIOException &e) {
                            std::cerr << "I/O error while saving offset" << std::endl;
                        }
                    }
                    break;
                case MotionNotify:
                    // Ignore mouse movement unless a button is pressed (see above)
                    if (!mouseDragging)
                        break;
                    // Determine the new position in world coordinates
                    mouseDragNew = sf::Vector2f(event.xmotion.x, event.xmotion.y);
                    sf::Vector2f mouseDragDelta = mouseDragNew - mouseDragOld;
                    picOffset = picOffset + mouseDragDelta;
                    picOffset.x = std::max(picOffsetMin.x, std::min(0.0f, picOffset.x));
                    picOffset.y = std::max(picOffsetMin.y, std::min(0.0f, picOffset.y));
                    mouseDragOld = sf::Vector2f(event.xmotion.x, event.xmotion.y);
                    //std::cout << picOffset.x << " " << picOffset.y << std::endl;
                    window.clear();
                    //if (clock.getElapsedTime() >= sf::seconds(1.0f / 70.0f)) {
                    //	    clock.restart();
                    picSprite.setPosition(picOffset);
                    window.draw(picSprite);
                    window.display();
                    //   }
                    break;
            }
        }

        XCloseDisplay(display);
    }
};

int main() {
    std::string configFilePath;
    libconfig::Config config;

    try {
        configFilePath = getConfigDir().append("config.cfg");
    }
    catch(char const* str) {
        std::cerr << str << std::endl;
        return EXIT_FAILURE;
    }

    try {
        config.readFile(configFilePath.c_str());
    }
    catch(const libconfig::FileIOException &e) {
        std::cerr << "I/O error while config reading file: " << configFilePath << std::endl;
        return EXIT_FAILURE;
    }
    catch(const libconfig::ParseException &e) {
        std::cerr << "Parse error at " << e.getFile() << ":" << e.getLine()
            << " - " << e.getError() << std::endl;
        return EXIT_FAILURE;
    }

    std::list<WallpaperWindow> windows;

    XInitThreads();

    for (const Monitor& monitor : getMonitors()) {
        std::string wallpaperPath;
        if(config.lookupValue("wallpapers." + monitor.name, wallpaperPath)) {
            windows.emplace_back(monitor, wallpaperPath);
            std::cout << monitor.name << " " << wallpaperPath << std::endl;
        }
    }

    std::mutex m;
    std::condition_variable cond;
    std::list<std::thread> windowThreads;
    std::atomic<bool> exit;

    for (WallpaperWindow& window : windows) {
        windowThreads.emplace_back([&] { window.updateLoop(); exit = true; cond.notify_all(); });
    }

    std::unique_lock<std::mutex> lock{m};
    cond.wait(lock, [&] { return bool(exit); });

    for (std::thread& thread : windowThreads) {
        thread.join();
    }

    //WallpaperWindow window(monitors[0]);
    //window.updateLoop();
    return EXIT_SUCCESS;
}

// vim: et sw=4 ts=4
