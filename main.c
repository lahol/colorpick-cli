#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <memory.h>
#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

struct {
    unsigned long int size;
    int show_version;
    int show_help;
    int multiple;
    char format[256];
} config;

struct Sample {
    unsigned long int size;
    unsigned int depth;
    size_t data_length;
    unsigned char *data;
};

Display *x_display = NULL;
int x_screen = 0;
Window x_root_window = 0;

void config_set_defaults(void)
{
    config.show_version = 0;
    config.show_help = 0;
    config.multiple = 0;
    config.size = 1;
    strcpy(config.format, "html");
}

/* read command line parameters; return 0 if everything’s fine */
int init(int argc, char **argv)
{
    int o, option_index = 0;

    struct option long_options[] = {
        { "version", no_argument, NULL, 'v' },
        { "help", no_argument, NULL, 'h' },
        { "size", required_argument, NULL, 's' },
        { "format", required_argument, NULL, 'f' },
        { "multiple", no_argument, NULL, 'm' },
        { NULL, 0, NULL, 0 }
    };

    while ((o = getopt_long(argc, argv, "vhs:f:m", long_options, &option_index)) != -1) {
        if ((char)o == 'v')
            config.show_version = 1;
        else if ((char)o == 'h')
            config.show_help = 1;
        else if ((char)o == 's')
            config.size = strtoul(optarg, NULL, 10);
        else if ((char)o == 'm')
            config.multiple = 1;
        else if ((char)o == 'f') {
            strncpy(config.format, optarg, 255);
            config.format[255] = '\0';
        }
        else
            return 1;
    }

    return 0;
}

int x_init(void)
{
    x_display = XOpenDisplay(NULL);
    if (x_display == NULL)
        return 1;

    x_screen = DefaultScreen(x_display);
    x_root_window = RootWindow(x_display, x_screen);

    return 0;
}

void get_pointer_position(int *x, int *y)
{
    Window rr, cr;
    int wrx, wry;
    unsigned int mr;

    XQueryPointer(x_display, x_root_window, &rr, &cr, x, y, &wrx, &wry, &mr);
}

void init_sample(struct Sample *sample, unsigned long int size, unsigned int depth)
{
    if (sample == NULL)
        return;

    sample->data_length = (size_t)((depth>>3) + (depth & 0x07));
    sample->size = size ? size : 1;
    sample->depth = depth;
    sample->data = malloc(sizeof(unsigned char) * sample->size * sample->size * sample->data_length);
    memset(sample->data, 0, sizeof(unsigned char) * sample->size * sample->size * sample->data_length);
}

void free_sample(struct Sample *sample)
{
    if (sample == NULL)
        return;

    if (sample->data != NULL)
        free(sample->data);

    memset(sample, 0, sizeof(struct Sample));
}

static inline unsigned int pop_count(uint32_t mask)
{
    unsigned int count;
    for (count = 0; mask; ++count)
        mask &= (mask -1);
    return count;
}

static inline void characterize_mask(uint32_t mask, int *width, int *shift)
{
    if (width) *width = pop_count(mask);
    if (shift) *shift = pop_count(((mask - 1) & ~mask)) & 31;
}

void img_to_sample(XImage *img, struct Sample *sample)
{
    if (img == NULL || sample == NULL || sample->data == NULL)
        return;
    unsigned int i, j;
    unsigned long int color;

    int red_shift, green_shift, blue_shift;
    characterize_mask(img->red_mask, NULL, &red_shift);
    characterize_mask(img->green_mask, NULL, &green_shift);
    characterize_mask(img->blue_mask, NULL, &blue_shift);

    for (i = 0; i < sample->size; ++i)
        for (j = 0; j < sample->size; ++j) {
            color = XGetPixel(img, i, j);
            sample->data[((i * sample->size + j) << 2) + 1] =
                (uint8_t)((color & img->red_mask) >> red_shift);
            sample->data[((i * sample->size + j) << 2) + 2] =
                (uint8_t)((color & img->green_mask) >> green_shift);
            sample->data[((i * sample->size + j) << 2) + 3] =
                (uint8_t)((color & img->blue_mask) >> blue_shift);
         }
}

int get_sample(Display *display, Window root_win, int x, int y, unsigned long int size, struct Sample *sample)
{
    if (sample == NULL)
        return 1;

    XImage *img = XGetImage(display, root_win, x, y, 1, 1, XAllPlanes(), ZPixmap);

    if (img == NULL)
        return 1;

    init_sample(sample, 1, 32);

    img_to_sample(img, sample);

    XFree(img);
    return 0;
}

/* return sample value as 00rrggbb, using some algorithm (arithmetic mean, weighted arithmetic mean, …) */
uint32_t get_sample_value(struct Sample *sample, int mean_type)
{
    if (sample == NULL || sample->data == NULL)
        return 0;
    if (sample->depth != 32)
        return 0;

    uint32_t accum_red = 0, accum_green = 0, accum_blue = 0;
    uint8_t mean_red, mean_green, mean_blue;

    uint32_t i, len;

    switch (mean_type) {
        case 0:
            len = sample->size * sample->size;
            for (i = 0; i < len; ++i) {
                accum_red   += sample->data[(i<<2) + 1];
                accum_green += sample->data[(i<<2) + 2];
                accum_blue  += sample->data[(i<<2) + 3];
            }
            mean_red = (uint8_t)(((double)accum_red)/len + 0.5f);
            mean_green = (uint8_t)(((double)accum_green)/len + 0.5f);
            mean_blue = (uint8_t)(((double)accum_blue)/len + 0.5f);
            break;
        default:
            return 0;
    }

    return ((mean_red << 16) | (mean_green << 8) | (mean_blue));
}

void output_color(uint32_t color)
{
    uint8_t red   = (color & 0x00ff0000) >> 16;
    uint8_t green = (color & 0x0000ff00) >> 8;
    uint8_t blue  = color & 0x000000ff;

    fprintf(stdout, "#%02x%02x%02x\n", red & 0xff, green & 0xff, blue & 0xff);
}

void print_version(void)
{
    fprintf(stdout, "colorpick-cli %s\n© 2014 Holger Langenau\n", COLORPICKVERSION);
}

void print_help(void)
{
}

void output_color_from_pos(int x, int y)
{
    struct Sample sample;
    uint32_t color;

    get_sample(x_display, x_root_window, x, y, 1, &sample);
    color = get_sample_value(&sample, 0);
    free_sample(&sample);

    output_color(color);

}

int main(int argc, char **argv)
{
    if (init(argc, argv) != 0)
        return 1;

    if (config.show_version) {
        print_version();
        return 0;
    }

    if (config.show_help) {
        print_help();
        return 0;
    }

    if (x_init() != 0)
        return 1;

    XGrabPointer(x_display, x_root_window,
            False, ButtonReleaseMask, GrabModeAsync, GrabModeAsync, x_root_window, None, CurrentTime);

    XEvent event;
    while (1) {
        XNextEvent(x_display, &event);
        if (event.type == ButtonRelease) {
            if (event.xbutton.button == 1) {
                output_color_from_pos(event.xbutton.x_root, event.xbutton.y_root);
                if (!config.multiple)
                    break;
            }
            else if (event.xbutton.button == 3) {
                break;
            }

        }
    }

    XUngrabPointer(x_display, CurrentTime);

    return 0;
}
