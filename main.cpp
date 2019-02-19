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

class WallpaperWindow {
    sf::RenderWindow window;
    sf::Texture picTexture;
    sf::Sprite picSprite;
    sf::Vector2f picOffset;
    sf::Vector2f picOffsetMin;

    public:
    WallpaperWindow(const Monitor monitor) :
        window(sf::VideoMode(monitor.width, monitor.height), "SFML works!"),
        picOffset(sf::Vector2f(0, 0))
    {
        window.setVisible(false);

        window.setFramerateLimit(60);
        window.setVerticalSyncEnabled(true);

        Display* display = XOpenDisplay(NULL);
        Window win = window.getSystemHandle();
        x11_window_set_desktop(display, win);
        x11_window_set_borderless(display, win);
        x11_window_set_below(display, win);
        x11_window_set_sticky(display, win);
        x11_window_set_skip_taskbar(display, win);
        x11_window_set_skip_pager(display, win);
        XCloseDisplay(display);

        window.setPosition(sf::Vector2i(monitor.x, monitor.y));
        window.setSize(sf::Vector2u(monitor.width, monitor.height));

        sf::View view;
        view.reset(sf::FloatRect(0, 0, monitor.width, monitor.height));
        window.setView(view);

        window.setVisible(true);

        picTexture.loadFromFile("./example1.png");
        //picTexture.loadFromFile("./example2.jpg");
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

        window.setActive(false);
    }

    void updateLoop() {
        sf::Vector2f mouseDragOld;
        sf::Vector2f mouseDragNew;
        bool mouseDragging = false;

        while (window.isOpen())
        {
            sf::Event event;
            while (window.pollEvent(event)) {
                switch (event.type) {
                    case sf::Event::Closed:
                        window.close();
                        break;
                    case sf::Event::MouseButtonPressed:
                        // Mouse button is pressed, get the position and set moving as active
                        if (event.mouseButton.button == 0) {
                            //moving = true;
                            //oldPos = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y));
                            mouseDragging = true;
                            mouseDragOld = sf::Vector2f(event.mouseButton.x, event.mouseButton.y);
                        }
                        break;
                    case  sf::Event::MouseButtonReleased:
                        // Mouse button is released, no longer move
                        if (event.mouseButton.button == 0) {
                            //moving = false;
                            mouseDragging = false;
                        }
                        break;
                    case sf::Event::MouseMoved:
                        {
                            // Ignore mouse movement unless a button is pressed (see above)
                            //if (!moving)
                            //    break;
                            if (!mouseDragging)
                                break;
                            // Determine the new position in world coordinates
                            //const sf::Vector2f newPos = window.mapPixelToCoords(sf::Vector2i(event.mouseMove.x, event.mouseMove.y));
                            mouseDragNew = sf::Vector2f(event.mouseMove.x, event.mouseMove.y);
                            sf::Vector2f mouseDragDelta = mouseDragNew - mouseDragOld;
                            picOffset = picOffset + mouseDragDelta;
                            picOffset.x = std::max(picOffsetMin.x, std::min(0.0f, picOffset.x));
                            picOffset.y = std::max(picOffsetMin.y, std::min(0.0f, picOffset.y));
                            mouseDragOld = sf::Vector2f(event.mouseMove.x, event.mouseMove.y);
                            std::cout << picOffset.x << " " << picOffset.y << std::endl;
                            
                            break;
                        }
                    default:
                        break;
                }
            }

            window.clear();
            picSprite.setPosition(picOffset);
            window.draw(picSprite);
            window.display();
        }

        window.setActive(false);
    }
};

int main() {
    std::list<WallpaperWindow> windows;

    XInitThreads();
    
    for (const Monitor& monitor : getMonitors()) {
        windows.emplace_back(monitor);
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
    return 0;
}
