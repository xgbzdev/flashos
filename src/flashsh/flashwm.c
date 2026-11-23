/*
 * flashwm - a ~60 line window manager for FlashOS.
 * Every window is forced to fill the whole screen with no decorations.
 * F12 or Ctrl+Q closes the running SWF and returns to the FlashOS prompt.
 */
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>

static int ignore_errors(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }

static void grab(Display *d, Window root, KeySym sym, unsigned mods) {
    KeyCode kc = XKeysymToKeycode(d, sym);
    unsigned extra[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask}; /* caps / numlock */
    for (int i = 0; i < 4; i++)
        XGrabKey(d, kc, mods | extra[i], root, True, GrabModeAsync, GrabModeAsync);
}

int main(void) {
    Display *d = XOpenDisplay(NULL);
    if (!d) { fprintf(stderr, "flashwm: cannot open display\n"); return 1; }
    XSetErrorHandler(ignore_errors);
    int scr = DefaultScreen(d);
    Window root = RootWindow(d, scr);
    int W = DisplayWidth(d, scr), H = DisplayHeight(d, scr);

    XSetWindowBackground(d, root, BlackPixel(d, scr));
    XClearWindow(d, root);
    XSelectInput(d, root, SubstructureRedirectMask | SubstructureNotifyMask);
    grab(d, root, XK_F12, 0);
    grab(d, root, XK_q, ControlMask);
    XSync(d, False);

    for (;;) {
        XEvent e;
        XNextEvent(d, &e);
        if (e.type == MapRequest) {
            Window w = e.xmaprequest.window;
            XSetWindowBorderWidth(d, w, 0);
            XMoveResizeWindow(d, w, 0, 0, W, H);
            XMapRaised(d, w);
            XSetInputFocus(d, w, RevertToPointerRoot, CurrentTime);
        } else if (e.type == ConfigureRequest) {
            XWindowChanges wc = {.x = 0, .y = 0, .width = W, .height = H, .border_width = 0};
            XConfigureWindow(d, e.xconfigurerequest.window,
                             CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
        } else if (e.type == KeyPress) {
            /* tell flash-run this was a deliberate quit, not a crash */
            FILE *m = fopen("/run/flashos/user-quit", "w");
            if (m) fclose(m);
            Window r, p, *kids = NULL;
            unsigned n = 0;
            if (XQueryTree(d, root, &r, &p, &kids, &n)) {
                for (unsigned i = 0; i < n; i++) XKillClient(d, kids[i]);
                if (kids) XFree(kids);
            }
            XSync(d, False);
        }
    }
}
