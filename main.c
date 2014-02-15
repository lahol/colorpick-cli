#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int main(int argc, char **argv)
{
    Display *display;
    int screen;
    Window root;

    Window rr, cr;
    int wrx, wry;
    unsigned int mr;

    display = XOpenDisplay(0);
    screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    int ptr_x, ptr_y;
    unsigned long color;

    XQueryPointer(display, root, &rr, &cr, &ptr_x, &ptr_y, &wrx,
            &wry, &mr);

    XImage *img = XGetImage(display, root, ptr_x, ptr_y, 1, 1,
            XAllPlanes(), ZPixmap);

    if (img != NULL) {
        printf("got image @(%d, %d)\n", ptr_x, ptr_y);
        color = XGetPixel(img, 0, 0);
        printf("#%08lx\n", color);
        XFree(img);
    }
    else {
        printf("error getting image @(%d, %d)\n", ptr_x, ptr_y);
    }

    return 0;
}
